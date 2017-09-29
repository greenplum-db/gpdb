/*-------------------------------------------------------------------------
 *
 * cdbpersistentrelation.c
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbpersistentrelation.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/transam.h"
#include "access/xact.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "utils/palloc.h"
#include "storage/fd.h"
#include "storage/relfilenode.h"

#include "catalog/catalog.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_database.h"
#include "catalog/gp_persistent.h"
#include "cdb/cdbsharedoidsearch.h"
#include "access/persistentfilesysobjname.h"
#include "cdb/cdbdirectopen.h"
#include "cdb/cdbpersistentstore.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "cdb/cdbpersistentdatabase.h"
#include "cdb/cdbpersistentrelation.h"
#include "storage/itemptr.h"
#include "utils/hsearch.h"
#include "storage/shmem.h"
#include "utils/guc.h"
#include "storage/smgr.h"
#include "storage/ipc.h"
#include "utils/builtins.h"
#include "utils/faultinjector.h"

/*
 * This module is for generic relation file create and drop.
 *
 * For create, it makes the file-system create of an empty file fully transactional so
 * the relation file will be deleted even on system crash.  The relation file could be a heap,
 * index, or append-only (row- or column-store).
 */

typedef struct PersistentRelationSharedData
{

	PersistentFileSysObjSharedData fileSysObjSharedData;

} PersistentRelationSharedData;

#define PersistentRelationData_StaticInit {PersistentFileSysObjData_StaticInit}

typedef struct PersistentRelationData
{

	PersistentFileSysObjData fileSysObjData;

} PersistentRelationData;


/*
 * Global Variables
 */
PersistentRelationSharedData *persistentRelationSharedData = NULL;

PersistentRelationData persistentRelationData = PersistentRelationData_StaticInit;

static void
PersistentRelation_VerifyInitScan(void)
{
	if (persistentRelationSharedData == NULL)
		elog(PANIC, "Persistent relation information shared-memory not setup");

	PersistentFileSysObj_VerifyInitScan();
}

/* ----------------------------------------------------------------------------- */
/*  Helpers  */
/* ----------------------------------------------------------------------------- */

extern void
PersistentRelation_Reset(void)
{
	/* Currently, nothing to do. */
}


/* ------------------------------------------------------------------------------ */

int64
PersistentRelation_MyHighestSerialNum(void)
{
	return PersistentFileSysObj_MyHighestSerialNum(
												   PersistentFsObjType_RelationFile);
}

int64
PersistentRelation_CurrentMaxSerialNum(void)
{
	READ_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	int64		value;

	READ_PERSISTENT_STATE_ORDERED_LOCK;

	value = PersistentFileSysObj_CurrentMaxSerialNum(
													 PersistentFsObjType_RelationFile);

	READ_PERSISTENT_STATE_ORDERED_UNLOCK;

	return value;
}

static Oid	persistentRelationCheckTablespace;
static int32 persistentRelationCheckTablespaceUseCount;
static RelFileNode persistentRelationCheckTablespaceRelFileNode;

static bool
PersistentRelation_CheckTablespaceScanTupleCallback(
													ItemPointer persistentTid,
													int64 persistentSerialNum,
													Datum *values)
{
	RelFileNode relFileNode;
	int32		segmentFileNum;

	PersistentFileSysRelStorageMgr relationStorageManager;

	PersistentFileSysState state;

	int64		createMirrorDataLossTrackingSessionNum;

	MirroredObjectExistenceState mirrorExistenceState;

	MirroredRelDataSynchronizationState mirrorDataSynchronizationState;
	bool		mirrorBufpoolMarkedForScanIncrementalResync;
	int64		mirrorBufpoolResyncChangedPageCount;
	XLogRecPtr	mirrorBufpoolResyncCkptLoc;
	BlockNumber mirrorBufpoolResyncCkptBlockNum;

	int64		mirrorAppendOnlyLossEof;
	int64		mirrorAppendOnlyNewEof;

	PersistentFileSysRelBufpoolKind relBufpoolKind;

	TransactionId parentXid;
	int64		serialNum;

	GpPersistentRelationNode_GetValues(
									   values,
									   &relFileNode.spcNode,
									   &relFileNode.dbNode,
									   &relFileNode.relNode,
									   &segmentFileNum,
									   &relationStorageManager,
									   &state,
									   &createMirrorDataLossTrackingSessionNum,
									   &mirrorExistenceState,
									   &mirrorDataSynchronizationState,
									   &mirrorBufpoolMarkedForScanIncrementalResync,
									   &mirrorBufpoolResyncChangedPageCount,
									   &mirrorBufpoolResyncCkptLoc,
									   &mirrorBufpoolResyncCkptBlockNum,
									   &mirrorAppendOnlyLossEof,
									   &mirrorAppendOnlyNewEof,
									   &relBufpoolKind,
									   &parentXid,
									   &serialNum);

	if (state == PersistentFileSysState_Created &&
		relFileNode.spcNode == persistentRelationCheckTablespace)
	{
		persistentRelationCheckTablespaceUseCount++;
		if (persistentRelationCheckTablespaceUseCount == 1)
		{
			memcpy(&persistentRelationCheckTablespaceRelFileNode, &relFileNode, sizeof(RelFileNode));
		}
	}

	return true;
	/* Continue. */
}

void
PersistentRelation_CheckTablespace(
								   Oid tablespace,

								   int32 *useCount,

								   RelFileNode *exampleRelFileNode)
{
	persistentRelationCheckTablespace = tablespace;
	persistentRelationCheckTablespaceUseCount = 0;

	MemSet(&persistentRelationCheckTablespaceRelFileNode, 0, sizeof(RelFileNode));

	PersistentFileSysObj_Scan(
							  PersistentFsObjType_RelationFile,
							  PersistentRelation_CheckTablespaceScanTupleCallback);

	*useCount = persistentRelationCheckTablespaceUseCount;
	memcpy(exampleRelFileNode, &persistentRelationCheckTablespaceRelFileNode, sizeof(RelFileNode));
}

/* ----------------------------------------------------------------------------- */
/*  State Change  */
/* ----------------------------------------------------------------------------- */

/*
 * Indicate we intend to create a relation file as part of the current transaction.
 *
 * This function adds an entry in 'gp_persistent_relation_node' for either a new table (segment file
 * # 0) or a new segment file under AO table (segment file # > 0 for row/column-oriented AO) with a state
 * 'Create Pending'. An XLOG IntentToCreate record is generated that will guard the subsequent file-system
 * create in case the transaction aborts.
 *
 * Paramaters
 * -----------
 * relFileNode = The tablespace, database, and relation OIDs for the create
 * segmentFileNum = As the name implies (   0 for heap
 *                                       >= 0 for RO/CO AO as applicable)
 * relStorageMgr = Persistent Relation storage Manager
 * relBufpoolKind = Buffer pool type beneath corrosponding relation
 * TODO bufferPollBulkLoad = ???
 * TODO mirrorExistenceState = ???
 * TODO relDataSynchronizationState = ???
 * flushToXlog = If true, the XLOG record for this change will be flushed to disk.
 * TODO isLocalBuf = ???
 *
 * Return
 * ------
 * relationName = Name of the relation used for either debugging or to store in PendingDelete LL.
 * persistentTid = Resulting TID of the gp_persistent_rel_files tuple for the relation
 * serialNum = Resulting serial number for the relation.  Distinquishes the uses of the tuple
 */
void
PersistentRelation_AddCreatePending(
									RelFileNode *relFileNode,
									int32 segmentFileNum,
									PersistentFileSysRelStorageMgr relStorageMgr,
									PersistentFileSysRelBufpoolKind relBufpoolKind,
									bool bufferPoolBulkLoad,
									MirroredObjectExistenceState mirrorExistenceState,
									MirroredRelDataSynchronizationState relDataSynchronizationState,
									char *relationName,
									ItemPointer persistentTid,
									int64 *serialNum,
									bool flushToXLog,
									bool isLocalBuf)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;
	TransactionId topXid;

	XLogRecPtr	mirrorBufpoolResyncCkptLoc;

	Datum		values[Natts_gp_persistent_relation_node];

	if (RelFileNode_IsEmpty(relFileNode))
		elog(ERROR, "Invalid RelFileNode (0,0,0)");

	MemSet(&mirrorBufpoolResyncCkptLoc, 0, sizeof(XLogRecPtr));

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent relation '%s' because we are before persistence work",
				 relpath(*relFileNode));

		MemSet(persistentTid, 0, sizeof(ItemPointerData));
		*serialNum = 0;

		return;

		/*
		 * The initdb process will load the persistent table once we out of
		 * bootstrap mode.
		 */
	}

	/*
	 * Verify if the needed shared mem data structures for persistent tables
	 * are setup and inited
	 */
	PersistentRelation_VerifyInitScan();

	/* Setup the file system object name */
	PersistentFileSysObjName_SetRelationFile(
											 &fsObjName,
											 relFileNode,
											 segmentFileNum);

	topXid = GetTopTransactionId();

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	/*
	 * Create a values array which will be used to create a
	 * 'gp_persistent_relation_node' tuple
	 */
	GpPersistentRelationNode_SetDatumValues(
											values,
											relFileNode->spcNode,
											relFileNode->dbNode,
											relFileNode->relNode,
											segmentFileNum,
											relStorageMgr,
											(bufferPoolBulkLoad ?
											 PersistentFileSysState_BulkLoadCreatePending :
											 PersistentFileSysState_CreatePending),
											 /* createMirrorDataLossTrackingSessionNum */ 0,
											mirrorExistenceState,
											relDataSynchronizationState,
											 /* mirrorBufpoolMarkedForScanIncrementalResync */ false,
											 /* mirrorBufpoolResyncChangedPageCount */ 0,
											&mirrorBufpoolResyncCkptLoc,
											 /* mirrorBufpoolResyncCkptBlockNum */ 0,
											 /* mirrorAppendOnlyLossEof */ 0,
											 /* mirrorAppendOnlyNewEof */ 0,
											relBufpoolKind,
											topXid,
											 /* persistentSerialNum */ 0);
	/* This will be set by PersistentFileSysObj_AddTuple. */

	/*
	 * Add a new tuple to 'gp_persistent_relation_node' table for the new
	 * relation/segment file we intend to create. This will also create and
	 * apply a new persistent serial number.
	 */
	PersistentFileSysObj_AddTuple(
								  PersistentFsObjType_RelationFile,
								  values,
								  flushToXLog,
								  persistentTid,
								  serialNum);

	/*
	 * This XLOG must be generated under the persistent write-lock.
	 */
#ifdef MASTER_MIRROR_SYNC
	mmxlog_log_create_relfilenode(
								  relFileNode->spcNode,
								  relFileNode->dbNode,
								  relFileNode->relNode,
								  segmentFileNum);
#endif

	SIMPLE_FAULT_INJECTOR(FaultBeforePendingDeleteRelationEntry);

	/*
	 * We'll add an entry to the PendingDelete LinkedList (LL) to remeber what
	 * we created in this transaction (or sub-transaction). If the transaction
	 * aborts then we can search for all such entries in this LL and get rid
	 * of (delete) such relations or segment files on the disk.
	 *
	 * MPP-18228 To make adding 'Create Pending' entry to persistent table and
	 * adding to the PendingDelete list atomic
	 */
	PendingDelete_AddCreatePendingRelationEntry(
												&fsObjName,
												persistentTid,
												serialNum,
												relStorageMgr,
												relationName,
												isLocalBuf,
												bufferPoolBulkLoad);


	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
			 "Persistent relation: Add '%s', relation name '%s' in state 'Create Pending', relation storage manager '%s', mirror existence state '%s', relation data resynchronization state '%s', serial number " INT64_FORMAT " at TID %s",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 relationName,
			 PersistentFileSysRelStorageMgr_Name(relStorageMgr),
			 MirroredObjectExistenceState_Name(mirrorExistenceState),
			 MirroredRelDataSynchronizationState_Name(relDataSynchronizationState),
			 *serialNum,
			 ItemPointerToString(persistentTid));
}

void
PersistentRelation_AddCreated(
							  RelFileNode *relFileNode,
 /* The tablespace, database, and relation OIDs for the create. */
							  int32 segmentFileNum,
							  PersistentFileSysRelStorageMgr relStorageMgr,
							  PersistentFileSysRelBufpoolKind relBufpoolKind,
							  MirroredObjectExistenceState mirrorExistenceState,
							  MirroredRelDataSynchronizationState relDataSynchronizationState,
							  int64 mirrorAppendOnlyLossEof,
							  int64 mirrorAppendOnlyNewEof,
							  char *relationName,
							  ItemPointer persistentTid,
 /* Resulting TID of the gp_persistent_rel_files tuple for the relation. */
							  int64 *persistentSerialNum,

 /*
  * Resulting serial number for the relation.  Distinquishes the uses of the
  * tuple.
  */
							  bool flushToXLog)
 /* When true, the XLOG record for this change will be flushed to disk. */
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	XLogRecPtr	mirrorBufpoolResyncCkptLoc;

	Datum		values[Natts_gp_persistent_relation_node];

	if (RelFileNode_IsEmpty(relFileNode))
		elog(ERROR, "Invalid RelFileNode (0,0,0)");

	MemSet(&mirrorBufpoolResyncCkptLoc, 0, sizeof(XLogRecPtr));

	if (!Persistent_BeforePersistenceWork())
		elog(ERROR, "We can only add to persistent meta-data when special states");

	/* Verify PersistentFileSysObj_BuildInitScan has been called. */
	PersistentRelation_VerifyInitScan();

	PersistentFileSysObjName_SetRelationFile(
											 &fsObjName,
											 relFileNode,
											 segmentFileNum);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	GpPersistentRelationNode_SetDatumValues(
											values,
											relFileNode->spcNode,
											relFileNode->dbNode,
											relFileNode->relNode,
											segmentFileNum,
											relStorageMgr,
											PersistentFileSysState_Created,
											 /* createMirrorDataLossTrackingSessionNum */ 0,
											mirrorExistenceState,
											relDataSynchronizationState,
											 /* mirrorBufpoolMarkedForScanIncrementalResync */ false,
											 /* mirrorBufpoolResyncChangedPageCount */ 0,
											&mirrorBufpoolResyncCkptLoc,
											 /* mirrorBufpoolResyncCkptBlockNum */ 0,
											mirrorAppendOnlyLossEof,
											mirrorAppendOnlyNewEof,
											relBufpoolKind,
											InvalidTransactionId,
											 /* persistentSerialNum */ 0);
	/* This will be set by PersistentFileSysObj_AddTuple. */

	PersistentFileSysObj_AddTuple(
								  PersistentFsObjType_RelationFile,
								  values,
								  flushToXLog,
								  persistentTid,
								  persistentSerialNum);

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
			 "Persistent relation: Add '%s', relation name '%s', in state 'Created', relation storage manager '%s', mirror existence state '%s', relation data resynchronization state '%s', serial number " INT64_FORMAT " at TID %s",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 relationName,
			 PersistentFileSysRelStorageMgr_Name(relStorageMgr),
			 MirroredObjectExistenceState_Name(mirrorExistenceState),
			 MirroredRelDataSynchronizationState_Name(relDataSynchronizationState),
			 *persistentSerialNum,
			 ItemPointerToString(persistentTid));
}

/* ----------------------------------------------------------------------------- */
/*  Transaction End   */
/* ----------------------------------------------------------------------------- */

/*
 * Indicate the transaction commited and the relation is officially created.
 */
void
PersistentRelation_FinishBufferPoolBulkLoad(
											RelFileNode *relFileNode,
 /* The tablespace, database, and relation OIDs for the created relation. */
											ItemPointer persistentTid,
 /* TID of the gp_persistent_rel_files tuple for the relation. */
											int64 persistentSerialNum)
 /* Serial number for the relation.  Distinquishes the uses of the tuple. */
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	PersistentFileSysObjStateChangeResult stateChangeResult;

	if (RelFileNode_IsEmpty(relFileNode))
		elog(ERROR, "Invalid RelFileNode (0,0,0)");

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent relation '%s' because we are before persistence work",
				 relpath(*relFileNode));

		return;

		/*
		 * The initdb process will load the persistent table once we out of
		 * bootstrap mode.
		 */
	}

	PersistentRelation_VerifyInitScan();

	PersistentFileSysObjName_SetRelationFile(
											 &fsObjName,
											 relFileNode,
											  /* segmentFileNum */ 0);

	/* Do this check after skipping out if in bootstrap mode. */
	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent '%s' tuple for 'Created' is invalid (0,0)",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName));

	if (persistentSerialNum == 0)
		elog(ERROR, "Persistent '%s' serial number for 'Created' is invalid (0)",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName));

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	stateChangeResult =
		PersistentFileSysObj_StateChange(
										 &fsObjName,
										 persistentTid,
										 persistentSerialNum,
										 PersistentFileSysState_CreatePending,
										  /* retryPossible */ false,
										  /* flushToXlog */ false,
										  /* oldState */ NULL,
										  /* verifiedActionCallback */ NULL);

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
			 "Persistent relation: '%s' changed state from 'Bulk Load Create Pending' to 'Create Pending', serial number " INT64_FORMAT " at TID %s (State-Change result '%s')",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 PersistentFileSysObjStateChangeResult_Name(stateChangeResult));
}

/*
 * Indicate the transaction commited and the relation is officially created.
 */
void
PersistentRelation_Created(
						   RelFileNode *relFileNode,
 /* The tablespace, database, and relation OIDs for the created relation. */
						   int32 segmentFileNum,
						   ItemPointer persistentTid,
 /* TID of the gp_persistent_rel_files tuple for the relation. */
						   int64 persistentSerialNum,
 /* Serial number for the relation.  Distinquishes the uses of the tuple. */
						   bool retryPossible)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	PersistentFileSysObjStateChangeResult stateChangeResult;

	if (RelFileNode_IsEmpty(relFileNode))
		elog(ERROR, "Invalid RelFileNode (0,0,0)");

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent relation '%s' because we are before persistence work",
				 relpath(*relFileNode));

		return;

		/*
		 * The initdb process will load the persistent table once we out of
		 * bootstrap mode.
		 */
	}

	PersistentRelation_VerifyInitScan();

	PersistentFileSysObjName_SetRelationFile(
											 &fsObjName,
											 relFileNode,
											 segmentFileNum);

	/* Do this check after skipping out if in bootstrap mode. */
	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent '%s' tuple for 'Created' is invalid (0,0)",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName));

	if (persistentSerialNum == 0)
		elog(ERROR, "Persistent '%s' serial number for 'Created' is invalid (0)",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName));

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

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

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
			 "Persistent relation: '%s' changed state from 'Create Pending' to 'Created', serial number " INT64_FORMAT " at TID %s (State-Change result '%s')",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 PersistentFileSysObjStateChangeResult_Name(stateChangeResult));
}

/*
 * Set mirror for all relation files.
 */
void
PersistentRelation_AddMirrorAll(int16 pridbid, int16 mirdbid)
{
	Relation	rel;
	HeapScanDesc scandesc;
	HeapTuple	tuple;

	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	if (Persistent_BeforePersistenceWork())
		elog(ERROR, "persistent table changes forbidden");

	rel = heap_open(GpPersistentRelationNodeRelationId,
					AccessExclusiveLock);
	scandesc = heap_beginscan(rel, SnapshotNow, 0, NULL);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	while ((tuple = heap_getnext(scandesc, ForwardScanDirection)) != NULL)
	{
		Form_gp_persistent_relation_node form =
		(Form_gp_persistent_relation_node) GETSTRUCT(tuple);
		Oid			tblspcoid = form->tablespace_oid;
		Oid			dboid = form->database_oid;
		Oid			relfilenode_oid = form->relfilenode_oid;
		int32		segment_file_num = form->segment_file_num;
		int64		serial = form->persistent_serial_num;

		PersistentFileSysObjName fsObjName;
		RelFileNode node;

		node.spcNode = tblspcoid;
		node.dbNode = dboid;
		node.relNode = relfilenode_oid;

		PersistentFileSysObjName_SetRelationFile(&fsObjName,
												 &node,
												 segment_file_num);

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

void
PersistentRelation_RemoveSegment(int16 dbid, bool ismirror)
{
	Relation	rel;
	HeapScanDesc scandesc;
	HeapTuple	tuple;

	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	if (Persistent_BeforePersistenceWork())
		elog(ERROR, "persistent table changes forbidden");

	rel = heap_open(GpPersistentRelationNodeRelationId,
					AccessExclusiveLock);
	scandesc = heap_beginscan(rel, SnapshotNow, 0, NULL);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	while ((tuple = heap_getnext(scandesc, ForwardScanDirection)) != NULL)
	{
		Form_gp_persistent_relation_node form =
		(Form_gp_persistent_relation_node) GETSTRUCT(tuple);
		Oid			tblspcoid = form->tablespace_oid;
		Oid			dboid = form->database_oid;
		Oid			relfilenode_oid = form->relfilenode_oid;
		int32		segment_file_num = form->segment_file_num;
		int64		serial = form->persistent_serial_num;

		PersistentFileSysObjName fsObjName;
		RelFileNode node;

		node.spcNode = tblspcoid;
		node.dbNode = dboid;
		node.relNode = relfilenode_oid;

		PersistentFileSysObjName_SetRelationFile(&fsObjName,
												 &node,
												 segment_file_num);

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
PersistentRelation_ActivateStandby(int16 oldmaster, int16 newmaster)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	Relation	rel;
	HeapScanDesc scandesc;
	HeapTuple	tuple;

	if (Persistent_BeforePersistenceWork())
		elog(ERROR, "persistent table changes forbidden");

	rel = heap_open(GpPersistentRelationNodeRelationId,
					AccessExclusiveLock);
	scandesc = heap_beginscan(rel, SnapshotNow, 0, NULL);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	while ((tuple = heap_getnext(scandesc, ForwardScanDirection)) != NULL)
	{
		Form_gp_persistent_relation_node form =
		(Form_gp_persistent_relation_node) GETSTRUCT(tuple);
		Oid			tblspcoid = form->tablespace_oid;
		Oid			dboid = form->database_oid;
		Oid			relfilenode_oid = form->relfilenode_oid;
		int32		segment_file_num = form->segment_file_num;
		int64		serial = form->persistent_serial_num;

		PersistentFileSysObjName fsObjName;
		RelFileNode node;

		node.spcNode = tblspcoid;
		node.dbNode = dboid;
		node.relNode = relfilenode_oid;

		PersistentFileSysObjName_SetRelationFile(&fsObjName,
												 &node,
												 segment_file_num);

		PersistentFileSysObj_ActivateStandby(&fsObjName,
											 &tuple->t_self,
											 serial,
											 oldmaster,
											 newmaster,
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
PersistentRelation_MarkDropPending(
								   RelFileNode *relFileNode,
 /* The tablespace, database, and relation OIDs for the drop. */
								   int32 segmentFileNum,
								   ItemPointer persistentTid,
 /* TID of the gp_persistent_rel_files tuple for the relation. */
								   int64 persistentSerialNum,
 /* Serial number for the relation.  Distinquishes the uses of the tuple. */
								   bool retryPossible)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	PersistentFileSysState oldState;

	PersistentFileSysObjStateChangeResult stateChangeResult;

	if (RelFileNode_IsEmpty(relFileNode))
		elog(ERROR, "Invalid RelFileNode (0,0,0)");

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent relation '%s' because we are before persistence work",
				 relpath(*relFileNode));

		return false;

		/*
		 * The initdb process will load the persistent table once we out of
		 * bootstrap mode.
		 */
	}

	PersistentRelation_VerifyInitScan();

	PersistentFileSysObjName_SetRelationFile(
											 &fsObjName,
											 relFileNode,
											 segmentFileNum);

	/* Do this check after skipping out if in bootstrap mode. */
	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent '%s' tuple for mark DROP pending is invalid (0,0)",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName));

	if (persistentSerialNum == 0)
		elog(ERROR, "Persistent '%s' serial number for mark DROP pending is invalid (0)",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName));

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	stateChangeResult =
		PersistentFileSysObj_StateChange(
										 &fsObjName,
										 persistentTid,
										 persistentSerialNum,
										 PersistentFileSysState_DropPending,
										 retryPossible,
										  /* flushToXlog */ false,
										 &oldState,
										  /* verifiedActionCallback */ NULL);

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
			 "Persistent relation: '%s' changed state from '%s' to 'Drop Pending', serial number " INT64_FORMAT " TID %s (State-Change result '%s')",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 PersistentFileSysObjState_Name(oldState),
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
PersistentRelation_MarkAbortingCreate(
									  RelFileNode *relFileNode,
 /* The tablespace, database, and relation OIDs for the aborting create. */
									  int32 segmentFileNum,
									  ItemPointer persistentTid,
 /* TID of the gp_persistent_rel_files tuple for the relation. */
									  int64 persistentSerialNum,
 /* Serial number for the relation.  Distinquishes the uses of the tuple. */
									  bool retryPossible)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	PersistentFileSysObjStateChangeResult stateChangeResult;

	if (RelFileNode_IsEmpty(relFileNode))
		elog(ERROR, "Invalid RelFileNode (0,0,0)");

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent relation '%s' because we are before persistence work",
				 relpath(*relFileNode));

		return false;

		/*
		 * The initdb process will load the persistent table once we out of
		 * bootstrap mode.
		 */
	}

	PersistentRelation_VerifyInitScan();

	PersistentFileSysObjName_SetRelationFile(
											 &fsObjName,
											 relFileNode,
											 segmentFileNum);

	/* Do this check after skipping out if in bootstrap mode. */
	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent '%s' tuple for mark DROP pending is invalid (0,0)",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName));

	if (persistentSerialNum == 0)
		elog(ERROR, "Persistent '%s' serial number for mark DROP pending is invalid (0)",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName));

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

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

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
			 "Persistent relation: '%s' changed state from 'Create Pending' to 'Aborting Create', serial number " INT64_FORMAT " at TID %s (State-Change result '%s')",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 PersistentFileSysObjStateChangeResult_Name(stateChangeResult));

	return stateChangeResult;
}

static void
PersistentRelation_DroppedVerifiedActionCallback(
												 PersistentFileSysObjName *fsObjName,
												 ItemPointer persistentTid,
 /* TID of the gp_persistent_rel_files tuple for the relation. */
												 int64 persistentSerialNum,
 /* Serial number for the relation.	Distinquishes the uses of the tuple. */
												 PersistentFileSysObjVerifyExpectedResult verifyExpectedResult)
{
	RelFileNode *relFileNode = PersistentFileSysObjName_GetRelFileNodePtr(fsObjName);
	int32		segmentFileNum = PersistentFileSysObjName_GetSegmentFileNum(fsObjName);

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
			mmxlog_log_remove_relfilenode(
										  relFileNode->spcNode,
										  relFileNode->dbNode,
										  relFileNode->relNode,
										  segmentFileNum);
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
PersistentRelation_Dropped(
						   RelFileNode *relFileNode,
 /* The tablespace, database, and relation OIDs for the dropped relation. */
						   int32 segmentFileNum,
						   ItemPointer persistentTid,
 /* TID of the gp_persistent_rel_files tuple for the relation. */
						   int64 persistentSerialNum)
 /* Serial number for the relation.  Distinquishes the uses of the tuple. */
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	PersistentFileSysState oldState;

	PersistentFileSysObjStateChangeResult stateChangeResult;

	if (RelFileNode_IsEmpty(relFileNode))
		elog(ERROR, "Invalid RelFileNode (0,0,0)");

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent relation '%s' because we are before persistence work",
				 relpath(*relFileNode));

		return;

		/*
		 * The initdb process will load the persistent table once we out of
		 * bootstrap mode.
		 */
	}

	PersistentRelation_VerifyInitScan();

	PersistentFileSysObjName_SetRelationFile(
											 &fsObjName,
											 relFileNode,
											 segmentFileNum);

	/* Do this check after skipping out if in bootstrap mode. */
	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent '%s' tuple for mark DROP pending is invalid (0,0)",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName));

	if (persistentSerialNum == 0)
		elog(ERROR, "Persistent '%s' serial number for mark DROP pending is invalid (0)",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName));

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	stateChangeResult =
		PersistentFileSysObj_StateChange(
										 &fsObjName,
										 persistentTid,
										 persistentSerialNum,
										 PersistentFileSysState_Free,
										  /* retryPossible */ false,
										  /* flushToXlog */ false,
										 &oldState,
										 PersistentRelation_DroppedVerifiedActionCallback);

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
			 "Persistent relation: '%s' changed state from '%s' to (Free), serial number " INT64_FORMAT " at TID %s (State-Change result '%s')",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 PersistentFileSysObjState_Name(oldState),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 PersistentFileSysObjStateChangeResult_Name(stateChangeResult));
}

void
PersistentRelation_MarkBufPoolRelationForScanIncrementalResync(
															   RelFileNode *relFileNode,
 /* The tablespace, database, and relation OIDs for the created relation. */
															   ItemPointer persistentTid,
 /* TID of the gp_persistent_rel_files tuple for the relation. */
															   int64 persistentSerialNum)
 /* Serial number for the relation.  Distinquishes the uses of the tuple. */
{
	PersistentFileSysObjName fsObjName;

	if (RelFileNode_IsEmpty(relFileNode))
		elog(ERROR, "Invalid RelFileNode (0,0,0)");

	if (GpPersistent_SkipXLogInfo(relFileNode->relNode))
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent relation '%s' because it is special",
				 relpath(*relFileNode));

		return;

		/*
		 * Resynchronize will always handle these relations as 'Scan
		 * Incremental'..
		 */
	}

	if (IsBootstrapProcessingMode())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent relation '%s' because we are in bootstrap mode",
				 relpath(*relFileNode));

		return;

		/*
		 * The initdb process will load the persistent table once we out of
		 * bootstrap mode.
		 */
	}

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent relation '%s' because we are before persistence work",
				 relpath(*relFileNode));

		return;

		/*
		 * The initdb process will load the persistent table once we out of
		 * bootstrap mode.
		 */
	}

	PersistentRelation_VerifyInitScan();

	PersistentFileSysObjName_SetRelationFile(
											 &fsObjName,
											 relFileNode,
											  /* segmentFileNum */ 0);

	/* Do this check after skipping out if in bootstrap mode. */
	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent '%s' tuple for mark physically truncated is invalid (0,0)",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName));

	if (persistentSerialNum == 0)
		elog(ERROR, "Persistent '%s' serial number for mark physcially truncated is invalid (0)",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName));

	PersistentFileSysObj_MarkBufPoolRelationForScanIncrementalResync(
																	 &fsObjName,
																	 persistentTid,
																	 persistentSerialNum,
																	  /* flushToXlog */ true);

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
			 "Persistent relation: '%s' marked physically truncated, serial number " INT64_FORMAT " at TID %s",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid));
}

/* ----------------------------------------------------------------------------- */
/*  Shmem */
/* ----------------------------------------------------------------------------- */

static Size
PersistentRelation_SharedDataSize(void)
{
	return MAXALIGN(sizeof(PersistentRelationSharedData));
}

/*
 * Return the required shared-memory size for this module.
 */
Size
PersistentRelation_ShmemSize(void)
{
	Size		size = 0;

	/* The shared-memory structure. */
	size = add_size(size, PersistentRelation_SharedDataSize());

	return size;
}

/*
 * Initialize the shared-memory for this module.
 */
void
PersistentRelation_ShmemInit(void)
{
	bool		found;

	/* Create the shared-memory structure. */
	persistentRelationSharedData =
		(PersistentRelationSharedData *)
		ShmemInitStruct("Mirrored Rel File Data",
						PersistentRelation_SharedDataSize(),
						&found);

	if (!found)
	{
		PersistentFileSysObj_InitShared(
										&persistentRelationSharedData->fileSysObjSharedData);
	}

	PersistentFileSysObj_Init(
							  &persistentRelationData.fileSysObjData,
							  &persistentRelationSharedData->fileSysObjSharedData,
							  PersistentFsObjType_RelationFile,
							   /* scanTupleCallback */ NULL);

	Assert(persistentRelationSharedData != NULL);
}
