/*-------------------------------------------------------------------------
 *
 * cdbpersistentfilesysobj.c
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbpersistentfilesysobj.c
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
#include "catalog/catalog.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_database.h"
#include "catalog/gp_persistent.h"
#include "catalog/gp_segment_config.h"
#include "cdb/cdbpersistentstore.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "cdb/cdbpersistentfilespace.h"
#include "cdb/cdbpersistenttablespace.h"
#include "cdb/cdbmirroredfilesysobj.h"
#include "cdb/cdbdirectopen.h"
#include "cdb/cdbfilerepservice.h"
#include "cdb/cdbfilerepresyncmanager.h"
#include "cdb/cdbresynchronizechangetracking.h"
#include "cdb/cdbmirroredbufferpool.h"
#include "cdb/cdbmirroredappendonly.h"
#include "cdb/cdbpersistentrecovery.h"
#include "cdb/cdbpersistentrelation.h"
#include "cdb/cdbpersistentdatabase.h"
#include "cdb/cdbglobalsequence.h"

#include "storage/itemptr.h"
#include "storage/lmgr.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/transam.h"
#include "utils/guc.h"
#include "utils/builtins.h"
#include "utils/faultinjector.h"

typedef struct PersistentFileSysObjPrivateSharedMemory
{
	bool	needInitScan;
} PersistentFileSysObjPrivateSharedMemory;

typedef struct ReadTupleForUpdateInfo
{
	PersistentFsObjType 		fsObjType;
	PersistentFileSysObjName 	*fsObjName;
	ItemPointerData				persistentTid;
	int64						persistentSerialNum;
} ReadTupleForUpdateInfo;

#define READTUPLE_FOR_UPDATE_ERRCONTEXT_DECLARE \
	ErrorContextCallback readTupleForUpdateErrContext; \
	ReadTupleForUpdateInfo readTupleForUpdateInfo;

#define READTUPLE_FOR_UPDATE_ERRCONTEXT_PUSHNAME(name, tid, serialNum) \
{ \
	readTupleForUpdateInfo.fsObjType = (name)->type; \
    readTupleForUpdateInfo.fsObjName = (name); \
	readTupleForUpdateInfo.persistentTid = *(tid); \
	readTupleForUpdateInfo.persistentSerialNum = serialNum; \
\
	/* Setup error traceback support for ereport() */ \
	readTupleForUpdateErrContext.callback = PersistentFileSysObj_ReadForUpdateErrContext; \
	readTupleForUpdateErrContext.arg = (void *) &readTupleForUpdateInfo; \
	readTupleForUpdateErrContext.previous = error_context_stack; \
	error_context_stack = &readTupleForUpdateErrContext; \
}

#define READTUPLE_FOR_UPDATE_ERRCONTEXT_PUSHTYPE(type, tid, serialNum) \
{ \
	readTupleForUpdateInfo.fsObjType = (type); \
    readTupleForUpdateInfo.fsObjName = NULL; \
	readTupleForUpdateInfo.persistentTid = *(tid); \
	readTupleForUpdateInfo.persistentSerialNum = (serialNum); \
\
	/* Setup error traceback support for ereport() */ \
	readTupleForUpdateErrContext.callback = PersistentFileSysObj_ReadForUpdateErrContext; \
	readTupleForUpdateErrContext.arg = (void *) &readTupleForUpdateInfo; \
	readTupleForUpdateErrContext.previous = error_context_stack; \
	error_context_stack = &readTupleForUpdateErrContext; \
}


#define READTUPLE_FOR_UPDATE_ERRCONTEXT_POP \
{ \
	/* Pop the error context stack */ \
	error_context_stack = readTupleForUpdateErrContext.previous; \
}

static PersistentFileSysObjPrivateSharedMemory *persistentFileSysObjPrivateSharedData = NULL;

static inline bool
PersistentFileSysObj_IsMirrorReCreate(
	int						state,
	int						mirrorExistenceState)
{
	return ((state == PersistentFileSysState_BulkLoadCreatePending ||
			 state == PersistentFileSysState_CreatePending ||
			 state == PersistentFileSysState_Created ||
			 state == PersistentFileSysState_JustInTimeCreatePending)
			&&
			(mirrorExistenceState ==
			 MirroredObjectExistenceState_MirrorDownBeforeCreate ||
			 mirrorExistenceState ==
			 MirroredObjectExistenceState_MirrorDownDuringCreate));
}

static inline bool
PersistentFileSysObj_IsMirrorReDrop(
	int						state,
	int						mirrorExistenceState)
{
	return ((state == PersistentFileSysState_DropPending ||
			 state == PersistentFileSysState_AbortingCreate)
			&&
			(mirrorExistenceState ==
			 MirroredObjectExistenceState_OnlyMirrorDropRemains));
}

void PersistentFileSysObj_InitShared(
	PersistentFileSysObjSharedData 	*fileSysObjSharedData)
{
	PersistentStore_InitShared(&fileSysObjSharedData->storeSharedData);
}

static PersistentFileSysObjData* fileSysObjDataArray[CountPersistentFsObjType];

static PersistentFileSysObjSharedData* fileSysObjSharedDataArray[CountPersistentFsObjType];

static void PersistentFileSysObj_PrintRelationFile(
	int						elevel,
	char					*prefix,
	ItemPointer 			persistentTid,
	Datum					*values)
{
	RelFileNode		relFileNode;
	int32			segmentFileNum;

	PersistentFileSysRelStorageMgr relationStorageManager;

	PersistentFileSysState	state;

	int64			createMirrorDataLossTrackingSessionNum;

	MirroredObjectExistenceState		mirrorExistenceState;

	MirroredRelDataSynchronizationState mirrorDataSynchronizationState;
	bool							mirrorBufpoolMarkedForScanIncrementalResync;
	int64							mirrorBufpoolResyncChangedPageCount;
	XLogRecPtr						mirrorBufpoolResyncCkptLoc;
	BlockNumber 					mirrorBufpoolResyncCkptBlockNum;

	int64					mirrorAppendOnlyLossEof;
	int64					mirrorAppendOnlyNewEof;

	PersistentFileSysRelBufpoolKind relBufpoolKind;

	TransactionId			parentXid;
	int64					persistentSerialNum;

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
									&persistentSerialNum);

	if (relationStorageManager == PersistentFileSysRelStorageMgr_BufferPool)
	{
		elog(elevel,
			 "%s gp_persistent_relation_node %s: %u/%u/%u, segment file #%d, relation storage manager '%s', persistent state '%s', create mirror data loss tracking session num " INT64_FORMAT ", "
			 "mirror existence state '%s', data synchronization state '%s', "
			 "Buffer Pool (marked for scan incremental resync = %s, "
			 "resync changed page count " INT64_FORMAT ", "
			 "resync checkpoint loc %s, resync checkpoint block num %u), "
			 "relation buffer pool kind %u, parent xid %u, "
			 "persistent serial num " INT64_FORMAT,
			 prefix,
			 ItemPointerToString(persistentTid),
			 relFileNode.spcNode,
			 relFileNode.dbNode,
			 relFileNode.relNode,
			 segmentFileNum,
			 PersistentFileSysRelStorageMgr_Name(relationStorageManager),
			 PersistentFileSysObjState_Name(state),
			 createMirrorDataLossTrackingSessionNum,
			 MirroredObjectExistenceState_Name(mirrorExistenceState),
			 MirroredRelDataSynchronizationState_Name(mirrorDataSynchronizationState),
			 (mirrorBufpoolMarkedForScanIncrementalResync ? "true" : "false"),
			 mirrorBufpoolResyncChangedPageCount,
			 XLogLocationToString(&mirrorBufpoolResyncCkptLoc),
			 (unsigned int)mirrorBufpoolResyncCkptBlockNum,
			 relBufpoolKind,
			 parentXid,
			 persistentSerialNum);
	}
	else
	{
		Assert(relationStorageManager == PersistentFileSysRelStorageMgr_AppendOnly);
		elog(elevel,
			 "%s gp_persistent_relation_node %s: %u/%u/%u, segment file #%d, relation storage manager '%s', persistent state '%s', create mirror data loss tracking session num " INT64_FORMAT ", "
			 "mirror existence state '%s', data synchronization state '%s', "
			 "Append-Only (loss EOF " INT64_FORMAT ", new EOF " INT64_FORMAT ", equal = %s), "
			 "relation buffer pool kind %u, parent xid %u, "
			 "persistent serial num " INT64_FORMAT ,
			 prefix,
			 ItemPointerToString(persistentTid),
			 relFileNode.spcNode,
			 relFileNode.dbNode,
			 relFileNode.relNode,
			 segmentFileNum,
			 PersistentFileSysRelStorageMgr_Name(relationStorageManager),
			 PersistentFileSysObjState_Name(state),
			 createMirrorDataLossTrackingSessionNum,
			 MirroredObjectExistenceState_Name(mirrorExistenceState),
			 MirroredRelDataSynchronizationState_Name(mirrorDataSynchronizationState),
			 mirrorAppendOnlyLossEof,
			 mirrorAppendOnlyNewEof,
			 ((mirrorAppendOnlyLossEof == mirrorAppendOnlyNewEof) ? "true" : "false"),
			 relBufpoolKind,
			 parentXid,
			 persistentSerialNum);
	}
}

static void PersistentFileSysObj_PrintDatabaseDir(
	int						elevel,
	char					*prefix,
	ItemPointer 			persistentTid,
	Datum					*values)
{
	DbDirNode		dbDirNode;

	PersistentFileSysState	state;

	int64			createMirrorDataLossTrackingSessionNum;

	MirroredObjectExistenceState		mirrorExistenceState;

	int32					reserved;
	TransactionId			parentXid;
	int64					persistentSerialNum;

	GpPersistentDatabaseNode_GetValues(
									values,
									&dbDirNode.tablespace,
									&dbDirNode.database,
									&state,
									&createMirrorDataLossTrackingSessionNum,
									&mirrorExistenceState,
									&reserved,
									&parentXid,
									&persistentSerialNum);

	elog(elevel,
		 "%s gp_persistent_database_node %s: %u/%u, persistent state '%s', create mirror data loss tracking session num " INT64_FORMAT ", "
		 "mirror existence state '%s', reserved %u, parent xid %u, "
		 "persistent serial num " INT64_FORMAT ,
		 prefix,
		 ItemPointerToString(persistentTid),
		 dbDirNode.tablespace,
		 dbDirNode.database,
		 PersistentFileSysObjState_Name(state),
		 createMirrorDataLossTrackingSessionNum,
		 MirroredObjectExistenceState_Name(mirrorExistenceState),
		 reserved,
		 parentXid,
		 persistentSerialNum);
}

static void PersistentFileSysObj_PrintTablespaceDir(
	int						elevel,
	char					*prefix,
	ItemPointer 			persistentTid,
	Datum					*values)
{
	Oid		filespaceOid;
	Oid		tablespaceOid;

	PersistentFileSysState	state;

	int64	createMirrorDataLossTrackingSessionNum;

	MirroredObjectExistenceState		mirrorExistenceState;

	int32					reserved;
	TransactionId			parentXid;
	int64					persistentSerialNum;

	GpPersistentTablespaceNode_GetValues(
									values,
									&filespaceOid,
									&tablespaceOid,
									&state,
									&createMirrorDataLossTrackingSessionNum,
									&mirrorExistenceState,
									&reserved,
									&parentXid,
									&persistentSerialNum);

	elog(elevel,
		 "%s gp_persistent_tablespace_node %s: tablespace %u (filespace %u), persistent state '%s', create mirror data loss tracking session num " INT64_FORMAT ", "
		 "mirror existence state '%s', reserved %u, parent xid %u, "
		 "persistent serial num " INT64_FORMAT ,
		 prefix,
		 ItemPointerToString(persistentTid),
		 tablespaceOid,
		 filespaceOid,
		 PersistentFileSysObjState_Name(state),
		 createMirrorDataLossTrackingSessionNum,
		 MirroredObjectExistenceState_Name(mirrorExistenceState),
		 reserved,
		 parentXid,
		 persistentSerialNum);
}

static void PersistentFileSysObj_PrintFilespaceDir(
	int						elevel,
	char					*prefix,
	ItemPointer 			persistentTid,
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
	int64					persistentSerialNum;

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
									&persistentSerialNum);

	char *filespaceLocation1;
	char *filespaceLocation2;

	PersistentFilespace_ConvertBlankPaddedLocation(
											&filespaceLocation1,
											locationBlankPadded1,
											/* isPrimary */ false);	// Always say false so we don't error out here.
	PersistentFilespace_ConvertBlankPaddedLocation(
											&filespaceLocation2,
											locationBlankPadded2,
											/* isPrimary */ false);	// Always say false so we don't error out here.
	elog(elevel,
		 "%s gp_persistent_filespace_node %s: filespace %u, #1 (dbid %u, location '%s'), #2 (dbid %u, location '%s'), persistent state '%s', create mirror data loss tracking session num " INT64_FORMAT ", "
		 "mirror existence state '%s', reserved %u, parent xid %u, "
		 "persistent serial num " INT64_FORMAT ,
		 prefix,
		 ItemPointerToString(persistentTid),
		 filespaceOid,
		 dbId1,
		 (filespaceLocation1 == NULL ? "<empty>" : filespaceLocation1),
		 dbId2,
		 (filespaceLocation2 == NULL ? "<empty>" : filespaceLocation2),
		 PersistentFileSysObjState_Name(state),
		 createMirrorDataLossTrackingSessionNum,
		 MirroredObjectExistenceState_Name(mirrorExistenceState),
		 reserved,
		 parentXid,
		 persistentSerialNum);

	if (filespaceLocation1 != NULL)
		pfree(filespaceLocation1);
	if (filespaceLocation2 != NULL)
		pfree(filespaceLocation2);
}

static void PersistentFileSysObj_InitOurs(
	PersistentFileSysObjData 	*fileSysObjData,
	PersistentFileSysObjSharedData 	*fileSysObjSharedData,
	PersistentFsObjType			fsObjType,
	int 						attNumPersistentState,
	int 						attNumMirrorExistenceState,
	int 						attNumParentXid)
{
	if (fsObjType < PersistentFsObjType_First ||
		fsObjType > PersistentFsObjType_Last)
		elog(ERROR, "Persistent file-system object type %d out-of-range",
		     fsObjType);

	fileSysObjDataArray[fsObjType] = fileSysObjData;
	fileSysObjSharedDataArray[fsObjType] = fileSysObjSharedData;

	fileSysObjData->fsObjType = fsObjType;
	fileSysObjData->attNumPersistentState = attNumPersistentState;
	fileSysObjData->attNumMirrorExistenceState = attNumMirrorExistenceState;
	fileSysObjData->attNumParentXid = attNumParentXid;
}

void PersistentFileSysObj_Init(
	PersistentFileSysObjData 			*fileSysObjData,
	PersistentFileSysObjSharedData 		*fileSysObjSharedData,
	PersistentFsObjType					fsObjType,
	PersistentStoreScanTupleCallback	scanTupleCallback)
{
	PersistentStoreData *storeData = &fileSysObjData->storeData;

	fileSysObjData->fsObjType = fsObjType;

	switch (fsObjType)
	{
	case PersistentFsObjType_RelationFile:
		PersistentStore_Init(
						storeData,
						"gp_persistent_relation_node",
						GpGlobalSequence_PersistentRelation,
						DirectOpen_GpPersistentRelationNodeOpenShared,
						DirectOpen_GpPersistentRelationNodeClose,
						scanTupleCallback,
						PersistentFileSysObj_PrintRelationFile,
						Natts_gp_persistent_relation_node,
						Anum_gp_persistent_relation_node_persistent_serial_num);

		PersistentFileSysObj_InitOurs(
						fileSysObjData,
						fileSysObjSharedData,
						fsObjType,
						Anum_gp_persistent_relation_node_persistent_state,
						Anum_gp_persistent_relation_node_mirror_existence_state,
						Anum_gp_persistent_relation_node_parent_xid);
		break;

	case PersistentFsObjType_DatabaseDir:
		PersistentStore_Init(
						storeData,
						"gp_persistent_database_node",
						GpGlobalSequence_PersistentDatabase,
						DirectOpen_GpPersistentDatabaseNodeOpenShared,
						DirectOpen_GpPersistentDatabaseNodeClose,
						scanTupleCallback,
						PersistentFileSysObj_PrintDatabaseDir,
						Natts_gp_persistent_database_node,
						Anum_gp_persistent_database_node_persistent_serial_num);
		
		PersistentFileSysObj_InitOurs(
						fileSysObjData,
						fileSysObjSharedData,
						fsObjType,
						Anum_gp_persistent_database_node_persistent_state,
						Anum_gp_persistent_database_node_mirror_existence_state,
						Anum_gp_persistent_database_node_parent_xid);
		break;

	case PersistentFsObjType_TablespaceDir:
		PersistentStore_Init(
						storeData,
						"gp_persistent_tablespace_node",
						GpGlobalSequence_PersistentTablespace,
						DirectOpen_GpPersistentTableSpaceNodeOpenShared,
						DirectOpen_GpPersistentTableSpaceNodeClose,
						scanTupleCallback,
						PersistentFileSysObj_PrintTablespaceDir,
						Natts_gp_persistent_tablespace_node,
						Anum_gp_persistent_tablespace_node_persistent_serial_num);

		PersistentFileSysObj_InitOurs(
						fileSysObjData,
						fileSysObjSharedData,
						fsObjType,
						Anum_gp_persistent_tablespace_node_persistent_state,
						Anum_gp_persistent_tablespace_node_mirror_existence_state,
						Anum_gp_persistent_tablespace_node_parent_xid);
		break;

	case PersistentFsObjType_FilespaceDir:
		PersistentStore_Init(
						storeData,
						"gp_persistent_filespace_node",
						GpGlobalSequence_PersistentFilespace,
						DirectOpen_GpPersistentFileSpaceNodeOpenShared,
						DirectOpen_GpPersistentFileSpaceNodeClose,
						scanTupleCallback,
						PersistentFileSysObj_PrintFilespaceDir,
						Natts_gp_persistent_filespace_node,
						Anum_gp_persistent_filespace_node_persistent_serial_num);
		
		PersistentFileSysObj_InitOurs(
						fileSysObjData,
						fileSysObjSharedData,
						fsObjType,
						Anum_gp_persistent_filespace_node_persistent_state,
						Anum_gp_persistent_filespace_node_mirror_existence_state,
						Anum_gp_persistent_filespace_node_parent_xid);
		break;

	default:
		elog(ERROR, "Unexpected persistent file-system object type: %d",
			 fsObjType);
	}
}

/*
 * Reset the persistent shared-memory free list heads and shared-memory hash tables.
 */
void PersistentFileSysObj_Reset(void)
{
	PersistentFsObjType	fsObjType;

	for (fsObjType = PersistentFsObjType_First;
		 fsObjType <= PersistentFsObjType_Last;
		 fsObjType++)
	{
		PersistentFileSysObjData		 *fileSysObjData;
		PersistentFileSysObjSharedData  *fileSysObjSharedData;

		PersistentFileSysObj_GetDataPtrs(
									 fsObjType,
									 &fileSysObjData,
									 &fileSysObjSharedData);

		switch (fsObjType)
		{
		case PersistentFsObjType_RelationFile:
			PersistentRelation_Reset();
			break;

		case PersistentFsObjType_DatabaseDir:
			PersistentDatabase_Reset();
			break;

		case PersistentFsObjType_TablespaceDir:
			PersistentTablespace_Reset();
			break;

		case PersistentFsObjType_FilespaceDir:
			/*
			 * Filespace persistent table is not reset (PersistentFilespace_Reset(); is not called)
			 * since it cannot be re-built on segments.
			 * 'gp_filespace' table does not exist on the segments by design.
			 */
			break;

		default:
			elog(ERROR, "Unexpected persistent file-system object type: %d",
				 fsObjType);
		}
	}
}

void PersistentFileSysObj_GetDataPtrs(
	PersistentFsObjType				fsObjType,

	PersistentFileSysObjData		**fileSysObjData,

	PersistentFileSysObjSharedData	**fileSysObjSharedData)
{
	if (fsObjType < PersistentFsObjType_First ||
		fsObjType > PersistentFsObjType_Last)
		elog(PANIC, "Persistent file-system object type %d out-of-range",
		     fsObjType);

	*fileSysObjData = fileSysObjDataArray[fsObjType];
	if (*fileSysObjData == NULL)
		elog(PANIC, "Persistent file-system object type %d memory not initialized",
		     fsObjType);
	if ((*fileSysObjData)->fsObjType != fsObjType)
		elog(PANIC, "Persistent file-system data's type %d doesn't match expected %d",
		     (*fileSysObjData)->fsObjType, fsObjType);

	*fileSysObjSharedData = fileSysObjSharedDataArray[fsObjType];
	if (*fileSysObjSharedData == NULL)
		elog(PANIC, "Persistent file-system object type %d shared-memory not initialized",
		     fsObjType);
}

static void PersistentFileSysObj_DoInitScan(void)
{
	PersistentFsObjType				fsObjType;

	PersistentFileSysObjData		*fileSysObjData;
	PersistentFileSysObjSharedData	*fileSysObjSharedData;

	Assert(persistentFileSysObjPrivateSharedData->needInitScan);

	for (fsObjType = PersistentFsObjType_First;
		 fsObjType <= PersistentFsObjType_Last;
		 fsObjType++)
	{
		 PersistentFileSysObj_GetDataPtrs(
									 fsObjType,
									 &fileSysObjData,
									 &fileSysObjSharedData);
		 PersistentStore_InitScanUnderLock(
						 &fileSysObjData->storeData,
						 &fileSysObjSharedData->storeSharedData);
	}

	 persistentFileSysObjPrivateSharedData->needInitScan = false;
}

void PersistentFileSysObj_BuildInitScan(void)
{
	if (persistentFileSysObjPrivateSharedData == NULL)
		elog(PANIC, "Persistent File-System Object shared-memory not initialized");

	if (!persistentFileSysObjPrivateSharedData->needInitScan)
	{
		// gp_persistent_build_db can be called multiple times.
		return;
	}

	PersistentFileSysObj_DoInitScan();
}

void PersistentFileSysObj_StartupInitScan(void)
{
	if (persistentFileSysObjPrivateSharedData == NULL)
		elog(PANIC, "Persistent File-System Object shared-memory not initialized");

	/*
	 * We only scan persistent meta-data from with Startup phase 1.
	 * Build is only done in special states.
	 */
	if (!persistentFileSysObjPrivateSharedData->needInitScan)
	{
		elog(PANIC, "We only scan persistent meta-data once during Startup Pass 1");
	}

	PersistentFileSysObj_DoInitScan();
}

void PersistentFileSysObj_VerifyInitScan(void)
{
	if (persistentFileSysObjPrivateSharedData == NULL)
		elog(PANIC, "Persistent File-System Object shared-memory not initialized");

	if (persistentFileSysObjPrivateSharedData->needInitScan)
	{
		elog(PANIC, "Persistent File-System Object meta-data not scanned into memory");
	}
}

void PersistentFileSysObj_FlushXLog(void)
{
	PersistentStore_FlushXLog();
}

void PersistentFileSysObj_Scan(
	PersistentFsObjType			fsObjType,

	PersistentStoreScanTupleCallback	scanTupleCallback)
{
	Assert(fsObjType >= PersistentFsObjType_First);
	Assert(fsObjType <= PersistentFsObjType_Last);

	PersistentStore_Scan(
					&(fileSysObjDataArray[fsObjType]->storeData),
					&(fileSysObjSharedDataArray[fsObjType]->storeSharedData),
					scanTupleCallback);

}

int64 PersistentFileSysObj_MyHighestSerialNum(
	PersistentFsObjType 	fsObjType)
{
	PersistentFileSysObjData 		*fileSysObjData;
	PersistentFileSysObjSharedData 	*fileSysObjSharedData;

	PersistentFileSysObj_GetDataPtrs(
								fsObjType,
								&fileSysObjData,
								&fileSysObjSharedData);

	return PersistentStore_MyHighestSerialNum(
								&fileSysObjData->storeData);
}

int64 PersistentFileSysObj_CurrentMaxSerialNum(
	PersistentFsObjType 	fsObjType)
{
	PersistentFileSysObjData 		*fileSysObjData;
	PersistentFileSysObjSharedData 	*fileSysObjSharedData;

	PersistentFileSysObj_GetDataPtrs(
								fsObjType,
								&fileSysObjData,
								&fileSysObjSharedData);

	return PersistentStore_CurrentMaxSerialNum(
								&fileSysObjSharedData->storeSharedData);
}

void PersistentFileSysObj_UpdateTuple(
	PersistentFsObjType		fsObjType,

	ItemPointer 			persistentTid,
				/* TID of the stored tuple. */

	Datum 					*values,

	bool					flushToXLog)
				/* When true, the XLOG record for this change will be flushed to disk. */
{
	Assert(fsObjType >= PersistentFsObjType_First);
	Assert(fsObjType <= PersistentFsObjType_Last);

	PersistentStore_UpdateTuple(
							&(fileSysObjDataArray[fsObjType]->storeData),
							&(fileSysObjSharedDataArray[fsObjType]->storeSharedData),
							persistentTid,
							values,
							flushToXLog);
}

void PersistentFileSysObj_ReplaceTuple(
	PersistentFsObjType		fsObjType,

	ItemPointer 			persistentTid,
				/* TID of the stored tuple. */

	HeapTuple				tuple,

	Datum 					*newValues,

	bool					*replaces,

	bool					flushToXLog)
				/* When true, the XLOG record for this change will be flushed to disk. */
{
	Assert(fsObjType >= PersistentFsObjType_First);
	Assert(fsObjType <= PersistentFsObjType_Last);

	PersistentStore_ReplaceTuple(
							&(fileSysObjDataArray[fsObjType]->storeData),
							&(fileSysObjSharedDataArray[fsObjType]->storeSharedData),
							persistentTid,
							tuple,
							newValues,
							replaces,
							flushToXLog);
}


void PersistentFileSysObj_ReadTuple(
	PersistentFsObjType			fsObjType,

	ItemPointer					readTid,

	Datum						*values,

	HeapTuple					*tupleCopy)
{
	Assert(fsObjType >= PersistentFsObjType_First);
	Assert(fsObjType <= PersistentFsObjType_Last);

	if (PersistentStore_IsZeroTid(readTid))
		elog(ERROR, "TID for fetch persistent '%s' tuple is invalid (0,0)",
			 PersistentFileSysObjName_TypeName(fsObjType));

	PersistentStore_ReadTuple(
						&(fileSysObjDataArray[fsObjType]->storeData),
						&(fileSysObjSharedDataArray[fsObjType]->storeSharedData),
						readTid,
						values,
						tupleCopy);
}

void PersistentFileSysObj_AddTuple(
	PersistentFsObjType			fsObjType,

	Datum						*values,

	bool						flushToXLog,
				/* When true, the XLOG record for this change will be flushed to disk. */

	ItemPointer 				persistentTid,
				/* TID of the stored tuple. */

	int64					*persistentSerialNum)
{
	Assert(fsObjType >= PersistentFsObjType_First);
	Assert(fsObjType <= PersistentFsObjType_Last);

	PersistentStore_AddTuple(
						&(fileSysObjDataArray[fsObjType]->storeData),
						&(fileSysObjSharedDataArray[fsObjType]->storeSharedData),
						values,
						flushToXLog,
						persistentTid,
						persistentSerialNum);
}

void PersistentFileSysObj_FreeTuple(
	PersistentFileSysObjData 		*fileSysObjData,

	PersistentFileSysObjSharedData 	*fileSysObjSharedData,

	PersistentFsObjType			fsObjType,

	ItemPointer 				persistentTid,
				/* TID of the stored tuple. */

	bool						flushToXLog)
				/* When true, the XLOG record for this change will be flushed to disk. */
{
	Datum *freeValues;

	Assert(fsObjType >= PersistentFsObjType_First);
	Assert(fsObjType <= PersistentFsObjType_Last);

	freeValues = (Datum*)palloc(fileSysObjData->storeData.numAttributes * sizeof(Datum));

	switch (fsObjType)
	{
	case PersistentFsObjType_RelationFile:
		{
			XLogRecPtr mirrorBufpoolResyncCkptLoc;

			MemSet(&mirrorBufpoolResyncCkptLoc, 0, sizeof(XLogRecPtr));
			GpPersistentRelationNode_SetDatumValues(
												freeValues,
												/* tablespaceOid */ 0,
												/* databaseOid */ 0,
												/* relFileNodeOid */ 0,
												/* segmentFileNum */ 0,
												PersistentFileSysRelStorageMgr_None,
												PersistentFileSysState_Free,
												/* createMirrorDataLossTrackingSessionNum */ 0,
												MirroredObjectExistenceState_None,
												MirroredRelDataSynchronizationState_None,
												/* mirrorBufpoolMarkedForScanIncrementalResync */ false,
												/* mirrorBufpoolResyncChangedPageCount */ 0,
												&mirrorBufpoolResyncCkptLoc,
												/* mirrorBufpoolResyncCkptBlockNum */ 0,
												/* mirrorAppendOnlyLossEof */ 0,
												/* mirrorAppendOnlyNewEof */ 0,
												PersistentFileSysRelBufpoolKind_None,
												InvalidTransactionId,
												/* persistentSerialNum */ 0);
		}
		break;

	case PersistentFsObjType_DatabaseDir:
		GpPersistentDatabaseNode_SetDatumValues(
											freeValues,
											/* tablespaceOid */ 0,
											/* databaseOid */ 0,
											PersistentFileSysState_Free,
											/* createMirrorDataLossTrackingSessionNum */ 0,
											MirroredObjectExistenceState_None,
											/* reserved */ 0,
											InvalidTransactionId,
											/* persistentSerialNum */ 0);
		break;

	case PersistentFsObjType_TablespaceDir:
		GpPersistentTablespaceNode_SetDatumValues(
											freeValues,
											/* filespaceOid */ 0,
											/* tablespaceOid */ 0,
											PersistentFileSysState_Free,
											/* createMirrorDataLossTrackingSessionNum */ 0,
											MirroredObjectExistenceState_None,
											/* reserved */ 0,
											InvalidTransactionId,
											/* persistentSerialNum */ 0);
		break;

	case PersistentFsObjType_FilespaceDir:
		{
			char	locationBlankFilled[FilespaceLocationBlankPaddedWithNullTermLen];

			MemSet(locationBlankFilled, ' ', FilespaceLocationBlankPaddedWithNullTermLen - 1);
			locationBlankFilled[FilespaceLocationBlankPaddedWithNullTermLen - 1] = '\0';

			GpPersistentFilespaceNode_SetDatumValues(
												freeValues,
												/* filespaceOid */ 0,
												/* dbId1 */ 0,
												locationBlankFilled,
												/* dbId2 */ 0,
												locationBlankFilled,
												PersistentFileSysState_Free,
												/* createMirrorDataLossTrackingSessionNum */ 0,
												MirroredObjectExistenceState_None,
												/* reserved */ 0,
												InvalidTransactionId,
												/* persistentSerialNum */ 0);
		}
		break;

	default:
		elog(ERROR, "Unexpected persistent file-system object type: %d",
			 fsObjType);
	}

	PersistentStore_FreeTuple(
						&fileSysObjData->storeData,
						&fileSysObjSharedData->storeSharedData,
						persistentTid,
						freeValues,
						flushToXLog);
}

static void
PersistentFileSysObj_ReadForUpdateErrContext(void *arg)
{
	ReadTupleForUpdateInfo *info = (ReadTupleForUpdateInfo*) arg;

	if (info->fsObjName != NULL)
	{
		errcontext(
			 "Reading tuple TID %s for possible update to persistent file-system object %s, persistent serial number " INT64_FORMAT,
			 ItemPointerToString(&info->persistentTid),
			 PersistentFileSysObjName_TypeAndObjectName(info->fsObjName),
			 info->persistentSerialNum);
	}
	else
	{
		errcontext(
			 "Reading tuple TID %s for possible update to persistent file-system object type %s, persistent serial number " INT64_FORMAT,
			 ItemPointerToString(&info->persistentTid),
			 PersistentFileSysObjName_TypeName(info->fsObjType),
			 info->persistentSerialNum);
	}
}

/*
 * errcontext_persistent_relation_state_change
 *
 * Add an errcontext() line showing the expected relation path, serial number,
 * expected state(s), next state, and persistent TID.
 */
static int
errcontext_persistent_relation_state_change(
	PersistentFileSysObjName	*fsObjName,

	ItemPointer 			persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */

	int64					serialNum,

	PersistentFileSysState	expectedState1,

	PersistentFileSysState	expectedState2,

	PersistentFileSysState	expectedState3,

	PersistentFileSysState	nextState)

{
	char states[100];

	if ((int)expectedState2 == -1)
		sprintf(states, "state '%s'",
		        PersistentFileSysObjState_Name(expectedState1));
	else if ((int)expectedState3 == -1)
		sprintf(states, "states '%s' or '%s'",
			    PersistentFileSysObjState_Name(expectedState1),
			    PersistentFileSysObjState_Name(expectedState2));
	else
		sprintf(states, "states '%s', '%s', or '%s'",
			    PersistentFileSysObjState_Name(expectedState1),
			    PersistentFileSysObjState_Name(expectedState2),
			    PersistentFileSysObjState_Name(expectedState3));

	errcontext(
		 "State change is for persistent '%s' to go from %s to state '%s' with serial number " INT64_FORMAT " at TID %s",
		 PersistentFileSysObjName_TypeAndObjectName(fsObjName),
		 states,
		 PersistentFileSysObjState_Name(nextState),
		 serialNum,
		 ItemPointerToString(persistentTid));

	return 0;
}

static PersistentFileSysObjVerifyExpectedResult PersistentFileSysObj_VerifyExpected(
	PersistentFileSysObjData 		*fileSysObjData,

	PersistentFileSysObjSharedData 	*fileSysObjSharedData,

	PersistentFileSysObjName		*fsObjName,

	ItemPointer 					persistentTid,
								/* TID of the gp_persistent_rel_files tuple for the rel file */

	int64							serialNum,

	Datum 							*values,

	PersistentFileSysState			expectedState1,

	PersistentFileSysState			expectedState2,

	PersistentFileSysState			expectedState3,

	PersistentFileSysState			nextState,

	bool							retryPossible,

	PersistentFileSysState			*oldState,

	MirroredObjectExistenceState	*currentMirrorExistenceState)
{
	PersistentFsObjType				fsObjType;

	PersistentFileSysObjName 		actualFsObjName;
	PersistentFileSysState			actualState;

	TransactionId				actualParentXid;
	int64						actualSerialNum;

	bool expectedStateMatch;

	int elevel;

	elevel = (gp_persistent_statechange_suppress_error ? WARNING : ERROR);

	fsObjType = fsObjName->type;

	if (Debug_persistent_print)
		(*fileSysObjData->storeData.printTupleCallback)(
									Persistent_DebugPrintLevel(),
									"VEFIFY EXPECTED",
									persistentTid,
									values);

	GpPersistent_GetCommonValues(
							fsObjType,
							values,
							&actualFsObjName,
							&actualState,
							currentMirrorExistenceState,
							&actualParentXid,
							&actualSerialNum);

	if (oldState != NULL)
		*oldState = actualState;

	expectedStateMatch = (actualState == expectedState1);
	if (!expectedStateMatch && ((int)expectedState2 != -1))
		expectedStateMatch = (actualState == expectedState2);
	if (!expectedStateMatch && ((int)expectedState3 != -1))
		expectedStateMatch = (actualState == expectedState3);

	if (actualState == PersistentFileSysState_Free)
	{
		if ((nextState == PersistentFileSysState_AbortingCreate ||
			 nextState == PersistentFileSysState_DropPending)
			 &&
			 retryPossible)
		{
			return PersistentFileSysObjVerifyExpectedResult_DeleteUnnecessary;
		}

		ereport(elevel,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Did not expect to find a 'Free' entry"),
				 errcontext_persistent_relation_state_change(
								 						fsObjName,
								 						persistentTid,
								 						serialNum,
								 						expectedState1,
								 						expectedState2,
								 						expectedState3,
								 						nextState)));
		return PersistentFileSysObjVerifyExpectedResult_ErrorSuppressed;
	}

	if (actualSerialNum != serialNum)
	{
		if ((nextState == PersistentFileSysState_AbortingCreate ||
			 nextState == PersistentFileSysState_DropPending)
			 &&
			 retryPossible)
		{
			return PersistentFileSysObjVerifyExpectedResult_DeleteUnnecessary;
		}

		ereport(elevel,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Found different serial number " INT64_FORMAT " than expected (persistent file-system object found is '%s', state '%s')",
				        actualSerialNum,
						PersistentFileSysObjName_TypeAndObjectName(&actualFsObjName),
				        PersistentFileSysObjState_Name(actualState)),
				 errcontext_persistent_relation_state_change(
								 						fsObjName,
								 						persistentTid,
								 						serialNum,
								 						expectedState1,
								 						expectedState2,
														expectedState3,
								 						nextState)));
		return PersistentFileSysObjVerifyExpectedResult_ErrorSuppressed;
	}

	if (!expectedStateMatch)
	{
		if (actualState == nextState
			&&
			retryPossible)
		{
			return PersistentFileSysObjVerifyExpectedResult_StateChangeAlreadyDone;
		}

		ereport(elevel,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Found persistent file-system object in unexpected state '%s'",
						PersistentFileSysObjState_Name(actualState)),
				 errcontext_persistent_relation_state_change(
														fsObjName,
														persistentTid,
														serialNum,
														expectedState1,
														expectedState2,
														expectedState3,
														nextState)));
		return PersistentFileSysObjVerifyExpectedResult_ErrorSuppressed;
	}

	return PersistentFileSysObjVerifyExpectedResult_StateChangeNeeded;
}

extern char *PersistentFileSysObjStateChangeResult_Name(
	PersistentFileSysObjStateChangeResult result)
{
	switch (result)
	{
	case PersistentFileSysObjStateChangeResult_DeleteUnnecessary:
		return "Delete Unnecessary";

	case PersistentFileSysObjStateChangeResult_StateChangeOk:
		return "State-Change OK";

	case PersistentFileSysObjStateChangeResult_ErrorSuppressed:
		return "Error Suppressed";

	default:
		return "Unknown";
	}
}

PersistentFileSysObjStateChangeResult PersistentFileSysObj_StateChange(
	PersistentFileSysObjName 	*fsObjName,

	ItemPointer					persistentTid,

	int64						persistentSerialNum,

	PersistentFileSysState		nextState,

	bool						retryPossible,

	bool						flushToXLog,

	PersistentFileSysState		*oldState,

	PersistentFileSysObjVerifiedActionCallback verifiedActionCallback)
{
	READTUPLE_FOR_UPDATE_ERRCONTEXT_DECLARE;

	PersistentFsObjType				fsObjType;

	PersistentFileSysObjData 		*fileSysObjData;
	PersistentFileSysObjSharedData 	*fileSysObjSharedData;
	PersistentFileSysRelStorageMgr   relStorageMgr = PersistentFileSysRelStorageMgr_None;

	Datum *values;

	HeapTuple tupleCopy;

	PersistentFileSysState	expectedState1;
	PersistentFileSysState	expectedState2 = -1;	// -1 means no second possibility.
	PersistentFileSysState	expectedState3 = -1;	// -1 means no third possibility.

	MirroredObjectExistenceState	currentMirrorExistenceState;
	MirroredObjectExistenceState	newMirrorExistenceState;

	PersistentFileSysObjVerifyExpectedResult verifyExpectedResult;

	PersistentFileSysState localOldState;

	bool badMirrorExistenceStateTransition;

	MirroredRelDataSynchronizationState newDataSynchronizationState = (MirroredRelDataSynchronizationState)-1;
	bool resetParentXid;

	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent '%s' state-change tuple is invalid (0,0)",
			 PersistentFileSysObjName_TypeName(fsObjName->type));

	if (persistentSerialNum == 0)
		elog(ERROR, "Persistent '%s' serial number for state-change is invalid (0)",
			 PersistentFileSysObjName_TypeName(fsObjName->type));

	if (oldState != NULL)
		*oldState = (PersistentFileSysState)-1;

	fsObjType = fsObjName->type;

	PersistentFileSysObj_GetDataPtrs(
								fsObjType,
								&fileSysObjData,
								&fileSysObjSharedData);

	values = (Datum*)palloc(fileSysObjData->storeData.numAttributes * sizeof(Datum));

	READTUPLE_FOR_UPDATE_ERRCONTEXT_PUSHNAME(fsObjName, persistentTid, persistentSerialNum);

	PersistentFileSysObj_ReadTuple(
							fsObjType,
							persistentTid,
							values,
							&tupleCopy);

	if (fsObjType == PersistentFsObjType_RelationFile)
		relStorageMgr =
		(PersistentFileSysRelStorageMgr)DatumGetInt16(values[Anum_gp_persistent_relation_node_relation_storage_manager
		- 1]);

	READTUPLE_FOR_UPDATE_ERRCONTEXT_POP;

	switch (nextState)
	{
	case PersistentFileSysState_CreatePending:
		Assert (fsObjType == PersistentFsObjType_RelationFile);

		expectedState1 = PersistentFileSysState_BulkLoadCreatePending;
		break;

	case PersistentFileSysState_Created:
		expectedState1 = PersistentFileSysState_CreatePending;

		if (fsObjType == PersistentFsObjType_DatabaseDir)
			expectedState2 = PersistentFileSysState_JustInTimeCreatePending;
		break;

	case PersistentFileSysState_DropPending:
		expectedState1 = PersistentFileSysState_Created;
		break;

	case PersistentFileSysState_AbortingCreate:
		expectedState1 = PersistentFileSysState_CreatePending;
		if (fsObjType == PersistentFsObjType_RelationFile)
		{
			expectedState2 = PersistentFileSysState_BulkLoadCreatePending;
		}
		break;

	case PersistentFileSysState_Free:
		expectedState1 = PersistentFileSysState_DropPending;
		expectedState2 = PersistentFileSysState_AbortingCreate;
		if (fsObjType == PersistentFsObjType_DatabaseDir)
			expectedState3 = PersistentFileSysState_JustInTimeCreatePending;
		break;

	default:
		elog(ERROR, "Unexpected persistent object next state: %d",
			 nextState);
        expectedState1 = MaxPersistentFileSysState; /* to appease optimized compilation */
	}

	verifyExpectedResult =
		PersistentFileSysObj_VerifyExpected(
									fileSysObjData,
									fileSysObjSharedData,
									fsObjName,
									persistentTid,
									persistentSerialNum,
									values,
									expectedState1,
									expectedState2,
									expectedState3,
									nextState,
									retryPossible,
									&localOldState,
									&currentMirrorExistenceState);
	if (oldState != NULL)
		*oldState = localOldState;

	if (verifiedActionCallback != NULL)
	{
		(*verifiedActionCallback)(
								fsObjName,
								persistentTid,
								persistentSerialNum,
								verifyExpectedResult,
								relStorageMgr);
	}

	switch (verifyExpectedResult)
	{
	case PersistentFileSysObjVerifyExpectedResult_DeleteUnnecessary:
		return PersistentFileSysObjStateChangeResult_DeleteUnnecessary;

	case PersistentFileSysObjVerifyExpectedResult_StateChangeAlreadyDone:
		return PersistentFileSysObjStateChangeResult_StateChangeOk;

	case PersistentFileSysObjVerifyExpectedResult_StateChangeNeeded:
		break;

	case PersistentFileSysObjVerifyExpectedResult_ErrorSuppressed:
		return PersistentFileSysObjStateChangeResult_ErrorSuppressed;

	default:
		elog(ERROR, "Unexpected persistent object verify expected result: %d",
			 verifyExpectedResult);
	}

	newMirrorExistenceState = currentMirrorExistenceState;	// Assume no change.
	badMirrorExistenceStateTransition = false;
	resetParentXid = false;	// Assume.

	switch (nextState)
	{
	case PersistentFileSysState_CreatePending:
		Assert (fsObjType == PersistentFsObjType_RelationFile);

		if (currentMirrorExistenceState == MirroredObjectExistenceState_NotMirrored ||
			currentMirrorExistenceState == MirroredObjectExistenceState_MirrorCreatePending ||
			currentMirrorExistenceState == MirroredObjectExistenceState_MirrorDownBeforeCreate ||
			currentMirrorExistenceState == MirroredObjectExistenceState_MirrorDownDuringCreate)
		{
			// Leave current mirror existence state alone.
		}
		else
		{
			badMirrorExistenceStateTransition = true;
		}

		/*
		 * Since bulk copy is copying data itself, we need to adjust the
		 * MirroredRelDataSynchronizationState based on the FileRep state...
		 */
		if (currentMirrorExistenceState == MirroredObjectExistenceState_MirrorCreatePending)
		{
			MirrorDataLossTrackingState 	currentMirrorDataLossTrackingState;
			int64							currentMirrorDataLossTrackingSessionNum;

			/*
			 * We are under the MirroredLock.
			 */
			currentMirrorDataLossTrackingState =
						FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
														&currentMirrorDataLossTrackingSessionNum);
			switch (currentMirrorDataLossTrackingState)
			{
			case MirrorDataLossTrackingState_MirrorNotConfigured:
				/*
				 * Rare case when you remove mirrors during bulk load.
				 */
				newDataSynchronizationState = MirroredRelDataSynchronizationState_None;
				break;

			case MirrorDataLossTrackingState_MirrorCurrentlyUpInSync:
			case MirrorDataLossTrackingState_MirrorCurrentlyUpInResync:
				newDataSynchronizationState = MirroredRelDataSynchronizationState_DataSynchronized;
				break;

			case MirrorDataLossTrackingState_MirrorDown:
				/*
				 * In Change Tracking mode -- need 'Full Copy'.
				 */
				newDataSynchronizationState = MirroredRelDataSynchronizationState_FullCopy;
				break;

			default:
				elog(ERROR, "unexpected mirror data loss tracking state: %d",
					 currentMirrorDataLossTrackingState);
				break;
			}
		}

		// Keep parent XID since we are still in '*Create Pending' state.
		break;

	case PersistentFileSysState_Created:
		if (currentMirrorExistenceState == MirroredObjectExistenceState_MirrorCreatePending)
		{
			newMirrorExistenceState = MirroredObjectExistenceState_MirrorCreated;
		}
		else if (currentMirrorExistenceState == MirroredObjectExistenceState_NotMirrored ||
				 currentMirrorExistenceState == MirroredObjectExistenceState_MirrorDownBeforeCreate ||
				 currentMirrorExistenceState == MirroredObjectExistenceState_MirrorDownDuringCreate)
		{
			// Leave current mirror existence state alone.
		}
		else
		{
			badMirrorExistenceStateTransition = true;
		}

		resetParentXid = true;
		break;

	case PersistentFileSysState_DropPending:
		if (currentMirrorExistenceState == MirroredObjectExistenceState_MirrorCreated ||
			currentMirrorExistenceState == MirroredObjectExistenceState_MirrorDownDuringCreate)
		{
			newMirrorExistenceState = MirroredObjectExistenceState_MirrorDropPending;
		}
		else if (currentMirrorExistenceState == MirroredObjectExistenceState_NotMirrored ||
				 currentMirrorExistenceState == MirroredObjectExistenceState_MirrorDownBeforeCreate)
		{
			// Leave current mirror existence state alone.
		}
		else
		{
			badMirrorExistenceStateTransition = true;
		}

		if (fsObjType == PersistentFsObjType_RelationFile)
		{
			newDataSynchronizationState = MirroredRelDataSynchronizationState_None;
		}

		resetParentXid = true;
		break;

	case PersistentFileSysState_AbortingCreate:
		if (currentMirrorExistenceState == MirroredObjectExistenceState_MirrorCreatePending ||
			currentMirrorExistenceState == MirroredObjectExistenceState_MirrorDownDuringCreate)
		{
			newMirrorExistenceState = MirroredObjectExistenceState_MirrorDropPending;
		}
		else if (currentMirrorExistenceState == MirroredObjectExistenceState_NotMirrored ||
				 currentMirrorExistenceState == MirroredObjectExistenceState_MirrorDownBeforeCreate)
		{
			// Leave current mirror existence state alone.
		}
		else
		{
			badMirrorExistenceStateTransition = true;
		}

		if (fsObjType == PersistentFsObjType_RelationFile)
		{
			newDataSynchronizationState = MirroredRelDataSynchronizationState_None;
		}

		resetParentXid = true;
		break;

	case PersistentFileSysState_Free:
		if (currentMirrorExistenceState == MirroredObjectExistenceState_NotMirrored ||
			currentMirrorExistenceState == MirroredObjectExistenceState_MirrorDownBeforeCreate ||
			currentMirrorExistenceState == MirroredObjectExistenceState_MirrorDropPending ||
			currentMirrorExistenceState == MirroredObjectExistenceState_OnlyMirrorDropRemains)
		{
			// These current mirror existence states are ok.
		}
		else
		{
			badMirrorExistenceStateTransition = true;
		}
		break;

	default:
		elog(ERROR, "Unexpected persistent object next state: %d",
			 nextState);
	}

	if (badMirrorExistenceStateTransition)
	{
		int elevel;

		elevel = (gp_persistent_statechange_suppress_error ? WARNING : ERROR);

		elog(elevel,
			 "In state transition of %s from current '%s' persistent state of to new '%s' persistent state "
			 " found unexpected current mirror existence state '%s' (persistent serial number " INT64_FORMAT ", TID %s)",
			 PersistentFileSysObjName_TypeAndObjectName(fsObjName),
			 PersistentFileSysObjState_Name(localOldState),
			 PersistentFileSysObjState_Name(nextState),
			 MirroredObjectExistenceState_Name(currentMirrorExistenceState),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid));
		return PersistentFileSysObjStateChangeResult_ErrorSuppressed;
	}

	if (nextState != PersistentFileSysState_Free)
	{
		Datum *newValues;
		bool *replaces;

		newValues = (Datum*)palloc0(fileSysObjData->storeData.numAttributes * sizeof(Datum));
		replaces = (bool*)palloc0(fileSysObjData->storeData.numAttributes * sizeof(bool));

		replaces[fileSysObjData->attNumPersistentState - 1] = true;
		newValues[fileSysObjData->attNumPersistentState - 1] = Int16GetDatum(nextState);

		if (newMirrorExistenceState != currentMirrorExistenceState)
		{
			replaces[fileSysObjData->attNumMirrorExistenceState - 1] = true;
			newValues[fileSysObjData->attNumMirrorExistenceState - 1] = Int16GetDatum(newMirrorExistenceState);
		}

		if (newDataSynchronizationState != (MirroredRelDataSynchronizationState)-1)
		{
			Assert (fsObjType == PersistentFsObjType_RelationFile);

			replaces[Anum_gp_persistent_relation_node_mirror_data_synchronization_state - 1] = true;
			newValues[Anum_gp_persistent_relation_node_mirror_data_synchronization_state - 1] =
														 Int16GetDatum(newDataSynchronizationState);
		}

		if (resetParentXid)
		{
			replaces[fileSysObjData->attNumParentXid - 1] = true;
			newValues[fileSysObjData->attNumParentXid - 1] = Int32GetDatum(InvalidTransactionId);
		}

		PersistentFileSysObj_ReplaceTuple(
									fsObjType,
									persistentTid,
									tupleCopy,
									newValues,
									replaces,
									flushToXLog);

		pfree(newValues);
		pfree(replaces);
	}
	else
	{
		heap_freetuple(tupleCopy);

		PersistentFileSysObj_FreeTuple(
									fileSysObjData,
									fileSysObjSharedData,
									fsObjType,
									persistentTid,
									flushToXLog);
	}

	if (Debug_persistent_print)
	{
		PersistentFileSysObj_ReadTuple(
								fsObjType,
								persistentTid,
								values,
								&tupleCopy);

		(*fileSysObjData->storeData.printTupleCallback)(
									Persistent_DebugPrintLevel(),
									"STATE CHANGE",
									persistentTid,
									values);

		heap_freetuple(tupleCopy);
	}

	pfree(values);

	return PersistentFileSysObjStateChangeResult_StateChangeOk;
}

void
PersistentFileSysObj_RemoveSegment(PersistentFileSysObjName *fsObjName,
								   ItemPointer persistentTid,
								   int64 persistentSerialNum,
								   int16 dbid,
								   bool ismirror,
								   bool flushToXLog)
{
	READTUPLE_FOR_UPDATE_ERRCONTEXT_DECLARE;

	PersistentFsObjType				fsObjType;

	PersistentFileSysObjData 		*fileSysObjData;
	PersistentFileSysObjSharedData 	*fileSysObjSharedData;

	Datum *values;

	HeapTuple tupleCopy;

	PersistentFileSysObjName		actualFsObjName;
	PersistentFileSysState			state;

	MirroredObjectExistenceState	mirrorExistenceState;

	TransactionId					parentXid;
	int64							serialNum;

	fsObjType = fsObjName->type;

	PersistentFileSysObj_GetDataPtrs(
								fsObjType,
								&fileSysObjData,
								&fileSysObjSharedData);

	values = (Datum*)palloc(fileSysObjData->storeData.numAttributes *
							sizeof(Datum));

	READTUPLE_FOR_UPDATE_ERRCONTEXT_PUSHNAME(fsObjName, persistentTid, persistentSerialNum);

	PersistentFileSysObj_ReadTuple(
							fsObjType,
							persistentTid,
							values,
							&tupleCopy);

	READTUPLE_FOR_UPDATE_ERRCONTEXT_POP;

	if (tupleCopy != NULL)
	{
		GpPersistent_GetCommonValues(
							fsObjType,
							values,
							&actualFsObjName,
							&state,
							&mirrorExistenceState,
							&parentXid,
							&serialNum);

		Datum *newValues;
		bool *replaces;

		if (ismirror
			&&
			(state == PersistentFileSysState_DropPending ||
			 state == PersistentFileSysState_AbortingCreate)
			 &&
			 mirrorExistenceState == MirroredObjectExistenceState_OnlyMirrorDropRemains)
		{
			PersistentFileSysState oldState;

			pfree(values);
			values = NULL;
			heap_freetuple(tupleCopy);

			/*
			 * The primary enters this state when it has successfully dropped the primary object but
			 * was unable to drop the mirror object because the mirror was down.
			 *
			 * Since we are letting go of the mirror, we can safely delete this persistent object.
			 */
			if (Debug_persistent_print)
				elog(Persistent_DebugPrintLevel(),
					 "PersistentFileSysObj_RemoveSegment: Removing '%s' as result of removing mirror, persistent state '%s', mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
					 PersistentFileSysObjName_ObjectName(fsObjName),
					 PersistentFileSysObjState_Name(state),
					 MirroredObjectExistenceState_Name(mirrorExistenceState),
					 persistentSerialNum,
					 ItemPointerToString(persistentTid));

			PersistentFileSysObj_StateChange(
										fsObjName,
										persistentTid,
										persistentSerialNum,
										PersistentFileSysState_Free,
										/* retryPossible */ false,
										/* flushToXLog */ false,
										&oldState,
										/* verifiedActionCallback */ NULL);
			return;
		}

		newValues = (Datum*)palloc0(fileSysObjData->storeData.numAttributes *
									sizeof(Datum));
		replaces = (bool*)palloc0(fileSysObjData->storeData.numAttributes *
								  sizeof(bool));

		if (ismirror)
		{
			replaces[fileSysObjData->attNumMirrorExistenceState - 1] = true;
			newValues[fileSysObjData->attNumMirrorExistenceState - 1] =
				Int32GetDatum(MirroredObjectExistenceState_NotMirrored);
		}


		if (fsObjType == PersistentFsObjType_FilespaceDir)
		{
			int dbidattnum;
			int locattnum;
			int dbid_1_attnum = Anum_gp_persistent_filespace_node_db_id_1;
			char ep[FilespaceLocationBlankPaddedWithNullTermLen];

			MemSet(ep, ' ', FilespaceLocationBlankPaddedWithNullTermLen - 1);
			ep[FilespaceLocationBlankPaddedWithNullTermLen - 1] = '\0';

			if (DatumGetInt16(values[dbid_1_attnum - 1]) == dbid)
			{
				dbidattnum = Anum_gp_persistent_filespace_node_db_id_1;
				locattnum = Anum_gp_persistent_filespace_node_location_1;
			}
			else
			{
				dbidattnum = Anum_gp_persistent_filespace_node_db_id_2;
				locattnum = Anum_gp_persistent_filespace_node_location_2;
			}

			replaces[dbidattnum - 1] = true;
			newValues[dbidattnum - 1] = Int16GetDatum(0);

			/*
			 * Although ep is allocated on the stack, CStringGetTextDatum()
			 * duplicates this and puts the result in palloc() allocated memory.
			 */
			replaces[locattnum - 1] = true;
			newValues[locattnum - 1] = CStringGetTextDatum(ep);
		}

		PersistentFileSysObj_ReplaceTuple(
									fsObjType,
									persistentTid,
									tupleCopy,
									newValues,
									replaces,
									flushToXLog);

		pfree(newValues);
		pfree(replaces);
	}
	pfree(values);
}

void
PersistentFileSysObj_ActivateStandby(PersistentFileSysObjName *fsObjName,
								   ItemPointer persistentTid,
								   int64 persistentSerialNum,
								   int16 oldmaster,
								   int16 newmaster,
								   bool flushToXLog)
{
	READTUPLE_FOR_UPDATE_ERRCONTEXT_DECLARE;

	PersistentFsObjType				fsObjType;

	PersistentFileSysObjData 		*fileSysObjData;
	PersistentFileSysObjSharedData 	*fileSysObjSharedData;

	Datum *values;

	HeapTuple tupleCopy;

	PersistentFileSysObjName		actualFsObjName;
	PersistentFileSysState			state;

	MirroredObjectExistenceState	mirrorExistenceState;

	TransactionId					parentXid;
	int64							serialNum;

	fsObjType = fsObjName->type;

	PersistentFileSysObj_GetDataPtrs(
								fsObjType,
								&fileSysObjData,
								&fileSysObjSharedData);

	values = (Datum*)palloc(fileSysObjData->storeData.numAttributes *
							sizeof(Datum));

	READTUPLE_FOR_UPDATE_ERRCONTEXT_PUSHNAME(fsObjName, persistentTid, persistentSerialNum);

	PersistentFileSysObj_ReadTuple(
							fsObjType,
							persistentTid,
							values,
							&tupleCopy);

	READTUPLE_FOR_UPDATE_ERRCONTEXT_POP;


	if (tupleCopy != NULL)
	{
		GpPersistent_GetCommonValues(
							fsObjType,
							values,
							&actualFsObjName,
							&state,
							&mirrorExistenceState,
							&parentXid,
							&serialNum);

		Datum *newValues;
		bool *replaces;
		int olddbidattnum;
		int newdbidattnum;

		newValues = (Datum*)palloc0(fileSysObjData->storeData.numAttributes *
									sizeof(Datum));
		replaces = (bool*)palloc0(fileSysObjData->storeData.numAttributes *
								  sizeof(bool));

		replaces[fileSysObjData->attNumParentXid - 1] = true;
		newValues[fileSysObjData->attNumParentXid - 1] =
			Int32GetDatum(InvalidTransactionId);

		replaces[fileSysObjData->attNumMirrorExistenceState - 1] = true;
		newValues[fileSysObjData->attNumMirrorExistenceState - 1] =
			Int32GetDatum(MirroredObjectExistenceState_NotMirrored);


		if (fsObjType == PersistentFsObjType_FilespaceDir)
		{
			int locattnum;
			int newLocattnum;
			char ep[FilespaceLocationBlankPaddedWithNullTermLen];
			int dbid_1_attnum = Anum_gp_persistent_filespace_node_db_id_1;
			int dbid_2_attnum = Anum_gp_persistent_filespace_node_db_id_2;

			MemSet(ep, ' ', FilespaceLocationBlankPaddedWithNullTermLen - 1);
			ep[FilespaceLocationBlankPaddedWithNullTermLen - 1] = '\0';

			if (DatumGetInt16(values[dbid_1_attnum - 1]) == oldmaster)
			{
				olddbidattnum = dbid_1_attnum;
				newdbidattnum = dbid_2_attnum;
				locattnum = Anum_gp_persistent_filespace_node_location_1;
				newLocattnum = Anum_gp_persistent_filespace_node_location_2;
			}
			else if (DatumGetInt16(values[dbid_2_attnum - 1]) == oldmaster)
			{
				olddbidattnum = dbid_2_attnum;
				newdbidattnum = dbid_1_attnum;
				locattnum = Anum_gp_persistent_filespace_node_location_2;
				newLocattnum = Anum_gp_persistent_filespace_node_location_1;
			}
			else
			{
				Insist(false);
			}

			/*
			 * Although ep is allocated on the stack, CStringGetTextDatum()
			 * duplicates this and puts the result in palloc() allocated memory.
			 */

			/* Replace the new master filespace location with the standby value. */
			replaces[newLocattnum - 1] = true;
			newValues[newLocattnum - 1] = values[locattnum - 1];

			replaces[locattnum - 1] = true;
			newValues[locattnum - 1] = CStringGetTextDatum(ep);
			replaces[olddbidattnum - 1] = true;
			newValues[olddbidattnum - 1] = Int16GetDatum(InvalidDbid);
			replaces[newdbidattnum - 1] = true;
			newValues[newdbidattnum - 1] = Int16GetDatum(newmaster);
		}

		PersistentFileSysObj_ReplaceTuple(
									fsObjType,
									persistentTid,
									tupleCopy,
									newValues,
									replaces,
									flushToXLog);

		pfree(newValues);
		pfree(replaces);
	}
	pfree(values);
}

void
PersistentFileSysObj_AddMirror(PersistentFileSysObjName *fsObjName,
							   ItemPointer persistentTid,
							   int64 persistentSerialNum,
							   int16 pridbid,
							   int16 mirdbid,
							   void *arg,
							   bool set_mirror_existence,
							   bool	flushToXLog)
{
	READTUPLE_FOR_UPDATE_ERRCONTEXT_DECLARE;

	PersistentFsObjType				fsObjType;

	PersistentFileSysObjData 		*fileSysObjData;
	PersistentFileSysObjSharedData 	*fileSysObjSharedData;

	Datum *values;

	HeapTuple tupleCopy;

	PersistentFileSysObjName		actualFsObjName;
	PersistentFileSysState			state;

	MirroredObjectExistenceState	mirrorExistenceState;

	TransactionId					parentXid;
	int64							serialNum;

	fsObjType = fsObjName->type;

	PersistentFileSysObj_GetDataPtrs(
								fsObjType,
								&fileSysObjData,
								&fileSysObjSharedData);

	values = (Datum*)palloc(fileSysObjData->storeData.numAttributes * sizeof(Datum));

	READTUPLE_FOR_UPDATE_ERRCONTEXT_PUSHNAME(fsObjName, persistentTid, persistentSerialNum);

	PersistentFileSysObj_ReadTuple(
							fsObjType,
							persistentTid,
							values,
							&tupleCopy);

	READTUPLE_FOR_UPDATE_ERRCONTEXT_POP;

	if (tupleCopy != NULL)
	{
		GpPersistent_GetCommonValues(
							fsObjType,
							values,
							&actualFsObjName,
							&state,
							&mirrorExistenceState,
							&parentXid,
							&serialNum);

		Datum *newValues;
		bool *replaces;
		bool needs_update = false;

		newValues = (Datum*)palloc0(fileSysObjData->storeData.numAttributes *
									sizeof(Datum));
		replaces = (bool*)palloc0(fileSysObjData->storeData.numAttributes *
								  sizeof(bool));

		if (set_mirror_existence)
		{
			replaces[fileSysObjData->attNumMirrorExistenceState - 1] = true;
			newValues[fileSysObjData->attNumMirrorExistenceState - 1] =
				Int32GetDatum(MirroredObjectExistenceState_MirrorDownBeforeCreate);
			needs_update = true;
		}

		if (fsObjType == PersistentFsObjType_FilespaceDir)
		{
			int dbidattnum;
			int locattnum;
			int dbid_1_attnum = Anum_gp_persistent_filespace_node_db_id_1;

			if (DatumGetInt16(values[dbid_1_attnum - 1]) == pridbid)
			{
				dbidattnum = Anum_gp_persistent_filespace_node_db_id_2;
				locattnum = Anum_gp_persistent_filespace_node_location_2;
			}
			else
			{
				dbidattnum = Anum_gp_persistent_filespace_node_db_id_1;
				locattnum = Anum_gp_persistent_filespace_node_location_1;
			}

			replaces[dbidattnum - 1] = true;
			newValues[dbidattnum - 1] = Int16GetDatum(mirdbid);

			/* arg must be of the fixed filespace location length */
			Assert(strlen((char *)arg) == FilespaceLocationBlankPaddedWithNullTermLen - 1);

			replaces[locattnum - 1] = true;
			newValues[locattnum - 1] = CStringGetTextDatum((char *)arg);

			needs_update = true;
		}

		/* only replace the tuple if we've changed it */
		if (needs_update)
		{
			PersistentFileSysObj_ReplaceTuple(fsObjType,
											  persistentTid,
											  tupleCopy,
											  newValues,
											  replaces,
											  flushToXLog);
		}
		else
		{
			heap_freetuple(tupleCopy);
		}

		pfree(newValues);
		pfree(replaces);
	}
	pfree(values);
}

void PersistentFileSysObj_RepairDelete(
	PersistentFsObjType			fsObjType,
	ItemPointer					persistentTid)
{
	PersistentFileSysObjData 		*fileSysObjData;
	PersistentFileSysObjSharedData 	*fileSysObjSharedData;

	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent repair delete tuple is invalid (0,0)");

	PersistentFileSysObj_GetDataPtrs(
								fsObjType,
								&fileSysObjData,
								&fileSysObjSharedData);

	/*
	 * There used to be a check here to perform ReadTuple and then
	 * validate if its free then error out. Removed the check as now
	 * simple_heap_delete can error out if trying to delete non
	 * existent or invisible tuple.
	 */
	PersistentFileSysObj_FreeTuple(
								fileSysObjData,
								fileSysObjSharedData,
								fsObjType,
								persistentTid,
								/* flushToXLog */ true);
}

static void
PersistentFileSysObj_DoMirrorValidation(
										PersistentFileSysObjName 	*fsObjName,
										PersistentFileSysRelStorageMgr relStorageMgr,
										ItemPointer					persistentTid,
										int64						persistentSerialNum)
{

	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent '%s' tuple for mirror validate is invalid (0,0)",
			 PersistentFileSysObjName_TypeName(fsObjName->type));

	if (persistentSerialNum == 0)
		elog(ERROR, "Persistent '%s' serial number for mirror validate is invalid (0)",
			 PersistentFileSysObjName_TypeName(fsObjName->type));

	switch (fsObjName->type)
	{
		case PersistentFsObjType_RelationFile:
		{
			char *primaryFilespaceLocation = NULL;
			char *mirrorFilespaceLocation = NULL;
			FileRepRelationType_e fileRepRelationType = FileRepRelationTypeNotSpecified;

			RelFileNode *relFileNode = PersistentFileSysObjName_GetRelFileNodePtr(fsObjName);
			int32 segmentFileNum = PersistentFileSysObjName_GetSegmentFileNum(fsObjName);

			Assert(relStorageMgr != PersistentFileSysRelStorageMgr_None);

			if (relStorageMgr == PersistentFileSysRelStorageMgr_BufferPool)
			{
				Assert(segmentFileNum == 0);

				fileRepRelationType = FileRepRelationTypeBufferPool;
			}
			else
			{
				Assert(relStorageMgr == PersistentFileSysRelStorageMgr_AppendOnly);

				fileRepRelationType = FileRepRelationTypeAppendOnly;
			}

			PersistentTablespace_GetPrimaryAndMirrorFilespaces(
															   relFileNode->spcNode,
															   &primaryFilespaceLocation,
															   &mirrorFilespaceLocation);

			FileRepPrimary_MirrorValidation(
											FileRep_GetRelationIdentifier(
																		  mirrorFilespaceLocation,
																		  *relFileNode,
																		  segmentFileNum),
											fileRepRelationType,
											FileRepValidationExistence);

			if (primaryFilespaceLocation != NULL)
				pfree(primaryFilespaceLocation);
			if (mirrorFilespaceLocation != NULL)
				pfree(mirrorFilespaceLocation);
		}
			break;

		case PersistentFsObjType_DatabaseDir:
		{
			char *primaryFilespaceLocation = NULL;
			char *mirrorFilespaceLocation = NULL;
			DbDirNode *dbDirNode = &fsObjName->variant.dbDirNode;

			PersistentTablespace_GetPrimaryAndMirrorFilespaces(
															   dbDirNode->tablespace,
															   &primaryFilespaceLocation,
															   &mirrorFilespaceLocation);

			FileRepPrimary_MirrorValidation(
											FileRep_GetDirDatabaseIdentifier(
																			 mirrorFilespaceLocation,
																			 *dbDirNode),
											FileRepRelationTypeDir,
											FileRepValidationExistence);

			if (primaryFilespaceLocation != NULL)
				pfree(primaryFilespaceLocation);
			if (mirrorFilespaceLocation != NULL)
				pfree(mirrorFilespaceLocation);
		}
			break;

		case PersistentFsObjType_TablespaceDir:
		{
			char *primaryFilespaceLocation = NULL;
			char *mirrorFilespaceLocation = NULL;

			PersistentTablespace_GetPrimaryAndMirrorFilespaces(
															   fsObjName->variant.tablespaceOid,
															   &primaryFilespaceLocation,
															   &mirrorFilespaceLocation);

			FileRepPrimary_MirrorValidation(
											FileRep_GetDirTablespaceIdentifier(
																			   mirrorFilespaceLocation,
																			   fsObjName->variant.tablespaceOid),
											FileRepRelationTypeDir,
											FileRepValidationExistence);

			if (primaryFilespaceLocation != NULL)
				pfree(primaryFilespaceLocation);
			if (mirrorFilespaceLocation != NULL)
				pfree(mirrorFilespaceLocation);
		}
			break;

		case PersistentFsObjType_FilespaceDir:
		{
			char *primaryFilespaceLocation = NULL;
			char *mirrorFilespaceLocation = NULL;

			PersistentFilespace_GetPrimaryAndMirror(
													fsObjName->variant.filespaceOid,
													&primaryFilespaceLocation,
													&mirrorFilespaceLocation);

			FileRepPrimary_MirrorValidation(
											FileRep_GetDirFilespaceIdentifier(
																			  mirrorFilespaceLocation),
											FileRepRelationTypeDir,
											FileRepValidationExistence);

			if (primaryFilespaceLocation != NULL)
				pfree(primaryFilespaceLocation);
			if (mirrorFilespaceLocation != NULL)
				pfree(mirrorFilespaceLocation);
		}
			break;

		default:
			elog(ERROR, "Unexpected persistent file-system object type: %d",
				 fsObjName->type);
	}
}

static void PersistentFileSysObj_DoMirrorReCreate(
	PersistentFileSysObjName 	*fsObjName,
	PersistentFileSysRelStorageMgr relStorageMgr,
	ItemPointer					persistentTid,
	int64						persistentSerialNum,
	MirrorDataLossTrackingState mirrorDataLossTrackingState,
	int64						mirrorDataLossTrackingSessionNum,
	bool						*mirrorDataLossOccurred)
{
	StorageManagerMirrorMode mirrorMode = StorageManagerMirrorMode_MirrorOnly;
	bool 					 ignoreAlreadyExists = true;
	int						 primaryError;

	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent '%s' tuple for mirror re-create is invalid (0,0)",
			 PersistentFileSysObjName_TypeName(fsObjName->type));

	if (persistentSerialNum == 0)
		elog(ERROR, "Persistent '%s' serial number for mirror re-create is invalid (0)",
			 PersistentFileSysObjName_TypeName(fsObjName->type));

	switch (fsObjName->type)
	{
	case PersistentFsObjType_RelationFile:
		{
			RelFileNode *relFileNode = PersistentFileSysObjName_GetRelFileNodePtr(fsObjName);
			int32 segmentFileNum = PersistentFileSysObjName_GetSegmentFileNum(fsObjName);

			Assert(relStorageMgr != PersistentFileSysRelStorageMgr_None);

			if (relStorageMgr == PersistentFileSysRelStorageMgr_BufferPool)
			{
				Assert(segmentFileNum == 0);

				MirroredBufferPool_MirrorReCreate(
											relFileNode,
											/* segmentFileNum */ 0,
											mirrorDataLossTrackingState,
											mirrorDataLossTrackingSessionNum,
											mirrorDataLossOccurred);
			}
			else
			{
				Assert(relStorageMgr == PersistentFileSysRelStorageMgr_AppendOnly);

				MirroredAppendOnly_MirrorReCreate(
											relFileNode,
											segmentFileNum,
											mirrorDataLossTrackingState,
											mirrorDataLossTrackingSessionNum,
											mirrorDataLossOccurred);
			}
		}
		break;

	case PersistentFsObjType_DatabaseDir:
		{
			DbDirNode *dbDirNode = &fsObjName->variant.dbDirNode;

			MirroredFileSysObj_CreateDbDir(
								dbDirNode,
								mirrorMode,
								ignoreAlreadyExists,
								&primaryError,
								mirrorDataLossOccurred);
		}
		break;

	case PersistentFsObjType_TablespaceDir:
		MirroredFileSysObj_CreateTablespaceDir(
							fsObjName->variant.tablespaceOid,
							mirrorMode,
							ignoreAlreadyExists,
							&primaryError,
							mirrorDataLossOccurred);
		break;

	case PersistentFsObjType_FilespaceDir:
		{
			char *primaryFilespaceLocation;
			char *mirrorFilespaceLocation;

			PersistentFilespace_GetPrimaryAndMirror(
								fsObjName->variant.filespaceOid,
								&primaryFilespaceLocation,
								&mirrorFilespaceLocation);

			MirroredFileSysObj_CreateFilespaceDir(
								fsObjName->variant.filespaceOid,
								primaryFilespaceLocation,
								mirrorFilespaceLocation,
								mirrorMode,
								ignoreAlreadyExists,
								&primaryError,
								mirrorDataLossOccurred);

			if (primaryFilespaceLocation != NULL)
				pfree(primaryFilespaceLocation);
			if (mirrorFilespaceLocation != NULL)
				pfree(mirrorFilespaceLocation);
		}
		break;

	default:
		elog(ERROR, "Unexpected persistent file-system object type: %d",
			 fsObjName->type);
	}
}

void PersistentFileSysObj_Created(
	PersistentFileSysObjName 	*fsObjName,
	ItemPointer				persistentTid,
	int64					persistentSerialNum,
	bool					retryPossible)
{
	if (!Persistent_BeforePersistenceWork())
	{
		if (PersistentStore_IsZeroTid(persistentTid))
			elog(ERROR, "TID for persistent '%s' tuple for created is invalid (0,0)",
				 PersistentFileSysObjName_TypeName(fsObjName->type));

		if (persistentSerialNum == 0)
			elog(ERROR, "Persistent '%s' serial number for created is invalid (0)",
				 PersistentFileSysObjName_TypeName(fsObjName->type));
	}

	switch (fsObjName->type)
	{
	case PersistentFsObjType_RelationFile:
		{
			RelFileNode *relFileNode = PersistentFileSysObjName_GetRelFileNodePtr(fsObjName);
			int32 segmentFileNum = PersistentFileSysObjName_GetSegmentFileNum(fsObjName);

			PersistentRelation_Created(
								relFileNode,
								segmentFileNum,
								persistentTid,
								persistentSerialNum,
								retryPossible);
		}
		break;

	case PersistentFsObjType_DatabaseDir:
		PersistentDatabase_Created(
							&fsObjName->variant.dbDirNode,
							persistentTid,
							persistentSerialNum,
							retryPossible);
		break;

	case PersistentFsObjType_TablespaceDir:
		PersistentTablespace_Created(
							fsObjName->variant.tablespaceOid,
							persistentTid,
							persistentSerialNum,
							retryPossible);
		break;

	case PersistentFsObjType_FilespaceDir:
		PersistentFilespace_Created(
							fsObjName->variant.filespaceOid,
							persistentTid,
							persistentSerialNum,
							retryPossible);
		break;


	default:
		elog(ERROR, "Unexpected persistent file-system object type: %d",
			 fsObjName->type);
	}
}

PersistentFileSysObjStateChangeResult PersistentFileSysObj_MarkAbortingCreate(
	PersistentFileSysObjName 	*fsObjName,
	ItemPointer				persistentTid,
	int64					persistentSerialNum,
	bool					retryPossible)
{
	PersistentFileSysObjStateChangeResult stateChangeResult;

	if (!Persistent_BeforePersistenceWork())
	{
		if (PersistentStore_IsZeroTid(persistentTid))
			elog(ERROR, "TID for persistent '%s' tuple for mark aborting CREATE is invalid (0,0)",
				 PersistentFileSysObjName_TypeName(fsObjName->type));

		if (persistentSerialNum == 0)
			elog(ERROR, "Persistent '%s' serial number for mark aborting CREATE is invalid (0)",
				 PersistentFileSysObjName_TypeName(fsObjName->type));
	}

	switch (fsObjName->type)
	{
	case PersistentFsObjType_RelationFile:
		{
			RelFileNode *relFileNode = PersistentFileSysObjName_GetRelFileNodePtr(fsObjName);
			int32 segmentFileNum = PersistentFileSysObjName_GetSegmentFileNum(fsObjName);

			stateChangeResult =
				PersistentRelation_MarkAbortingCreate(
								relFileNode,
								segmentFileNum,
								persistentTid,
								persistentSerialNum,
								retryPossible);
		}
		break;

	case PersistentFsObjType_DatabaseDir:
		stateChangeResult =
			PersistentDatabase_MarkAbortingCreate(
							&fsObjName->variant.dbDirNode,
							persistentTid,
							persistentSerialNum,
							retryPossible);
		break;

	case PersistentFsObjType_TablespaceDir:
		stateChangeResult =
			PersistentTablespace_MarkAbortingCreate(
							fsObjName->variant.tablespaceOid,
							persistentTid,
							persistentSerialNum,
							retryPossible);
		break;

	case PersistentFsObjType_FilespaceDir:
		stateChangeResult =
			PersistentFilespace_MarkAbortingCreate(
							fsObjName->variant.filespaceOid,
							persistentTid,
							persistentSerialNum,
							retryPossible);
		break;

	default:
		elog(ERROR, "Unexpected persistent file-system object type: %d",
			 fsObjName->type);
		stateChangeResult = PersistentFileSysObjStateChangeResult_None;
	}

	return stateChangeResult;
}

PersistentFileSysObjStateChangeResult PersistentFileSysObj_MarkDropPending(
	PersistentFileSysObjName 	*fsObjName,
	ItemPointer				persistentTid,
	int64					persistentSerialNum,
	bool					retryPossible)
{
	PersistentFileSysObjStateChangeResult stateChangeResult;

	if (!Persistent_BeforePersistenceWork())
	{
		if (PersistentStore_IsZeroTid(persistentTid))
			elog(ERROR, "TID for persistent '%s' tuple for mark DROP pending is invalid (0,0)",
				 PersistentFileSysObjName_TypeName(fsObjName->type));

		if (persistentSerialNum == 0)
			elog(ERROR, "Persistent '%s' serial number for mark DROP pending is invalid (0)",
				 PersistentFileSysObjName_TypeName(fsObjName->type));
	}

	switch (fsObjName->type)
	{
	case PersistentFsObjType_RelationFile:
		{
			RelFileNode *relFileNode = PersistentFileSysObjName_GetRelFileNodePtr(fsObjName);
			int32 segmentFileNum = PersistentFileSysObjName_GetSegmentFileNum(fsObjName);

			stateChangeResult =
				PersistentRelation_MarkDropPending(
								relFileNode,
								segmentFileNum,
								persistentTid,
								persistentSerialNum,
								retryPossible);
		}
		break;

	case PersistentFsObjType_DatabaseDir:
		stateChangeResult =
			PersistentDatabase_MarkDropPending(
							&fsObjName->variant.dbDirNode,
							persistentTid,
							persistentSerialNum,
							retryPossible);
		break;

	case PersistentFsObjType_TablespaceDir:
		stateChangeResult =
			PersistentTablespace_MarkDropPending(
							fsObjName->variant.tablespaceOid,
							persistentTid,
							persistentSerialNum,
							retryPossible);
		break;

	case PersistentFsObjType_FilespaceDir:
		stateChangeResult =
			PersistentFilespace_MarkDropPending(
							fsObjName->variant.tablespaceOid,
							persistentTid,
							persistentSerialNum,
							retryPossible);
		break;

	default:
		elog(ERROR, "Unexpected persistent file-system object type: %d",
			 fsObjName->type);
		stateChangeResult = PersistentFileSysObjStateChangeResult_None;
	}

	return stateChangeResult;
}

/*
 * Error context callback for errors occurring during PersistentFileSysObj_DropObject().
 */
static void
PersistentFileSysObj_DropObjectErrorCallback(void *arg)
{
	PersistentFileSysObjName *fsObjName = (PersistentFileSysObjName *) arg;

	errcontext("Dropping file-system object -- %s",
		       PersistentFileSysObjName_TypeAndObjectName(fsObjName));
}


void PersistentFileSysObj_DropObject(
	PersistentFileSysObjName 	*fsObjName,
	PersistentFileSysRelStorageMgr relStorageMgr,
	char						*relationName,
					/* For tracing only.  Can be NULL in some execution paths. */
	ItemPointer					persistentTid,
	int64						persistentSerialNum,
	bool 						ignoreNonExistence,
	bool						debugPrint,
	int							debugPrintLevel)
{
	READTUPLE_FOR_UPDATE_ERRCONTEXT_DECLARE;

	PersistentFsObjType				fsObjType;

	PersistentFileSysObjData 		*fileSysObjData;
	PersistentFileSysObjSharedData 	*fileSysObjSharedData;

	Datum *values;

	HeapTuple tupleCopy;

	PersistentFileSysObjName 		actualFsObjName;
	PersistentFileSysState		  	actualState;

	MirroredObjectExistenceState  	mirrorExistenceState;

	TransactionId					actualParentXid;
	int64							actualSerialNum;

	bool							suppressMirror;

	ErrorContextCallback errcontext;

	bool mirrorDataLossOccurred;

	/* Setup error traceback support for ereport() */
	errcontext.callback = PersistentFileSysObj_DropObjectErrorCallback;
	errcontext.arg = (void *) fsObjName;
	errcontext.previous = error_context_stack;
	error_context_stack = &errcontext;

	if (!Persistent_BeforePersistenceWork())
	{
		if (PersistentStore_IsZeroTid(persistentTid))
			elog(ERROR, "TID for persistent '%s' tuple for DROP is invalid (0,0)",
				 PersistentFileSysObjName_TypeName(fsObjName->type));

		if (persistentSerialNum == 0)
			elog(ERROR, "Persistent '%s' serial number for DROP is invalid (0)",
				 PersistentFileSysObjName_TypeName(fsObjName->type));
	}

	fsObjType = fsObjName->type;

	PersistentFileSysObj_GetDataPtrs(
								fsObjType,
								&fileSysObjData,
								&fileSysObjSharedData);

	values = (Datum*)palloc(fileSysObjData->storeData.numAttributes * sizeof(Datum));

	READTUPLE_FOR_UPDATE_ERRCONTEXT_PUSHNAME(fsObjName, persistentTid, persistentSerialNum);

	PersistentFileSysObj_ReadTuple(
							fsObjType,
							persistentTid,
							values,
							&tupleCopy);

	READTUPLE_FOR_UPDATE_ERRCONTEXT_POP;

	if (tupleCopy == NULL)
		elog(ERROR, "Expected persistent entry '%s' not to be in 'Free' state for DROP at TID %s",
			 PersistentFileSysObjName_TypeAndObjectName(fsObjName),
			 ItemPointerToString(persistentTid));

	GpPersistent_GetCommonValues(
							fsObjType,
							values,
							&actualFsObjName,
							&actualState,
							&mirrorExistenceState,
							&actualParentXid,
							&actualSerialNum);

	pfree(values);

	heap_freetuple(tupleCopy);

	if (persistentSerialNum != actualSerialNum)
		elog(ERROR, "Expected persistent entry '%s' serial number mismatch for DROP (expected " INT64_FORMAT ", found " INT64_FORMAT "), at TID %s",
			 PersistentFileSysObjName_TypeAndObjectName(fsObjName),
			 persistentSerialNum,
			 actualSerialNum,
			 ItemPointerToString(persistentTid));


	/*
	 * If we didn't try to create a mirror object because the mirror was already known to
	 * be down, then we can safely drop just the primary object.
	 */
	suppressMirror = (mirrorExistenceState == MirroredObjectExistenceState_MirrorDownBeforeCreate);

	if (debugPrint)
		elog(debugPrintLevel,
			 "PersistentFileSysObj_DropObject: before drop of %s, persistent serial number " INT64_FORMAT ", TID %s, suppressMirror %s",
			 PersistentFileSysObjName_TypeAndObjectName(fsObjName),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 (suppressMirror ? "true" : "false"));

	switch (fsObjType)
	{
	case PersistentFsObjType_RelationFile:
		{
			RelFileNode *relFileNode = PersistentFileSysObjName_GetRelFileNodePtr(fsObjName);
			int32 segmentFileNum = PersistentFileSysObjName_GetSegmentFileNum(fsObjName);

			if (!PersistentFileSysRelStorageMgr_IsValid(relStorageMgr))
				elog(ERROR, "Relation storage manager for persistent '%s' tuple for DROP is invalid (%d)",
					 PersistentFileSysObjName_TypeName(fsObjName->type),
					 relStorageMgr);

			MirroredFileSysObj_DropRelFile(
								relFileNode,
								segmentFileNum,
								relStorageMgr,
								relationName,
								/* isLocalBuf */ false,
								/* primaryOnly */ suppressMirror,
								ignoreNonExistence,
								&mirrorDataLossOccurred);


			if (debugPrint)
				elog(debugPrintLevel,
					 "PersistentFileSysObj_DropObject: after drop of %s, persistent serial number " INT64_FORMAT ", TID %s, suppressMirror %s and mirrorDataLossOccurred %s",
					 PersistentFileSysObjName_TypeAndObjectName(fsObjName),
					 persistentSerialNum,
					 ItemPointerToString(persistentTid),
					 (suppressMirror ? "true" : "false"),
					 (mirrorDataLossOccurred ? "true" : "false"));

			if (suppressMirror || !mirrorDataLossOccurred)
				PersistentRelation_Dropped(
									relFileNode,
									segmentFileNum,
									persistentTid,
									persistentSerialNum);
		}
		break;

	case PersistentFsObjType_DatabaseDir:
		{
			DbDirNode *dbDirNode = &fsObjName->variant.dbDirNode;

			MirroredFileSysObj_DropDbDir(
								dbDirNode,
								/* primaryOnly */ suppressMirror,
								/* mirrorOnly */ false,
								ignoreNonExistence,
								&mirrorDataLossOccurred);

			if (debugPrint)
				elog(debugPrintLevel,
					 "PersistentFileSysObj_DropObject: after drop of %s, persistent serial number " INT64_FORMAT ", TID %s, suppressMirror %s and mirrorDataLossOccurred %s",
					 PersistentFileSysObjName_TypeAndObjectName(fsObjName),
					 persistentSerialNum,
					 ItemPointerToString(persistentTid),
					 (suppressMirror ? "true" : "false"),
					 (mirrorDataLossOccurred ? "true" : "false"));

			if (suppressMirror || !mirrorDataLossOccurred)
				PersistentDatabase_Dropped(
									dbDirNode,
									persistentTid,
									persistentSerialNum);
		}
		break;

	case PersistentFsObjType_TablespaceDir:
		MirroredFileSysObj_DropTablespaceDir(
							fsObjName->variant.tablespaceOid,
							/* primaryOnly */ suppressMirror,
							/* mirrorOnly */ false,
							ignoreNonExistence,
							&mirrorDataLossOccurred);

		if (debugPrint)
			elog(debugPrintLevel,
				 "PersistentFileSysObj_DropObject: after drop of %s, persistent serial number " INT64_FORMAT ", TID %s, suppressMirror %s and mirrorDataLossOccurred %s",
				 PersistentFileSysObjName_TypeAndObjectName(fsObjName),
				 persistentSerialNum,
				 ItemPointerToString(persistentTid),
				 (suppressMirror ? "true" : "false"),
				 (mirrorDataLossOccurred ? "true" : "false"));

		if (suppressMirror || !mirrorDataLossOccurred)
			PersistentTablespace_Dropped(
								fsObjName->variant.tablespaceOid,
								persistentTid,
								persistentSerialNum);
		break;

	case PersistentFsObjType_FilespaceDir:
		{
			char *primaryFilespaceLocation;
			char *mirrorFilespaceLocation;

			PersistentFilespace_GetPrimaryAndMirror(
								fsObjName->variant.filespaceOid,
								&primaryFilespaceLocation,
								&mirrorFilespaceLocation);

			MirroredFileSysObj_DropFilespaceDir(
								fsObjName->variant.filespaceOid,
								primaryFilespaceLocation,
								mirrorFilespaceLocation,
								/* primaryOnly */ suppressMirror,
								/* mirrorOnly */ false,
								ignoreNonExistence,
								&mirrorDataLossOccurred);

			if (debugPrint)
				elog(debugPrintLevel,
					 "PersistentFileSysObj_DropObject: after drop of %s, persistent serial number " INT64_FORMAT ", TID %s, suppressMirror %s and mirrorDataLossOccurred %s",
					 PersistentFileSysObjName_TypeAndObjectName(fsObjName),
					 persistentSerialNum,
					 ItemPointerToString(persistentTid),
					 (suppressMirror ? "true" : "false"),
					 (mirrorDataLossOccurred ? "true" : "false"));

			if (suppressMirror || !mirrorDataLossOccurred)
				PersistentFilespace_Dropped(
									fsObjName->variant.filespaceOid,
									persistentTid,
									persistentSerialNum);

			if (primaryFilespaceLocation != NULL)
				pfree(primaryFilespaceLocation);
			if (mirrorFilespaceLocation != NULL)
				pfree(mirrorFilespaceLocation);
		}
		break;

	default:
		elog(ERROR, "Unexpected persistent file-system object type: %d",
			 fsObjName->type);
	}

	if (!suppressMirror && mirrorDataLossOccurred)
	{
		/*
		 * Leave this persistent object in 'Drop Pending' state with an indication that
		 * Re-Drop will be needed during Resynchronize.
		 */
		PersistentFileSysObj_ChangeMirrorState(
											fsObjName->type,
											persistentTid,
											persistentSerialNum,
											MirroredObjectExistenceState_OnlyMirrorDropRemains,
											MirroredRelDataSynchronizationState_None,
											/* flushToXLog */ true);

		if (debugPrint)
			elog(debugPrintLevel,
				 "PersistentFileSysObj_DropObject: leaving %s in '%s' persistent state and setting mirror existence state to '%s', persistent serial number " INT64_FORMAT ", TID %s",
				 PersistentFileSysObjName_TypeAndObjectName(fsObjName),
				 PersistentFileSysObjState_Name(PersistentFileSysState_DropPending),
				 MirroredObjectExistenceState_Name(MirroredObjectExistenceState_OnlyMirrorDropRemains),
				 persistentSerialNum,
				 ItemPointerToString(persistentTid));

	}

	/* Pop the error context stack */
	error_context_stack = errcontext.previous;
}

/*
 * This function performs database object (relation, tablespace etc) drops on the
 * corresponding mirror segment but does NO updates to persistent tables.
 *
 * Input parameters help this function to determine exact database object for drop
 * task or to find the exact persistent table tuple to mark it dropped/free.
 */
static void PersistentFileSysObj_DoMirrorReDrop(
	PersistentFileSysObjName 	*fsObjName,
	PersistentFileSysRelStorageMgr relStorageMgr,
	ItemPointer					persistentTid,
	int64						persistentSerialNum,
	MirrorDataLossTrackingState mirrorDataLossTrackingState,
	int64						mirrorDataLossTrackingSessionNum,
	bool						*mirrorDataLossOccurred)
{
	*mirrorDataLossOccurred = false;

	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent '%s' tuple for mirror re-drop is invalid (0,0)",
			 PersistentFileSysObjName_TypeName(fsObjName->type));

	if (persistentSerialNum == 0)
		elog(ERROR, "Persistent '%s' serial number for mirror re-drop is invalid (0)",
			 PersistentFileSysObjName_TypeName(fsObjName->type));

	switch (fsObjName->type)
	{
	case PersistentFsObjType_RelationFile:
		{
			RelFileNode *relFileNode = PersistentFileSysObjName_GetRelFileNodePtr(fsObjName);
			int32 segmentFileNum = PersistentFileSysObjName_GetSegmentFileNum(fsObjName);

			Assert(relStorageMgr != PersistentFileSysRelStorageMgr_None);

			if (relStorageMgr == PersistentFileSysRelStorageMgr_BufferPool)
			{
				Assert(segmentFileNum == 0);

				MirroredBufferPool_MirrorReDrop(
											relFileNode,
											segmentFileNum,
											mirrorDataLossTrackingState,
											mirrorDataLossTrackingSessionNum,
											mirrorDataLossOccurred);
			}
			else
			{
				Assert(relStorageMgr == PersistentFileSysRelStorageMgr_AppendOnly);

				MirroredAppendOnly_MirrorReDrop(
											relFileNode,
											segmentFileNum,
											mirrorDataLossTrackingState,
											mirrorDataLossTrackingSessionNum,
											mirrorDataLossOccurred);
			}

			if (*mirrorDataLossOccurred)
				return;
		}
		break;

	case PersistentFsObjType_DatabaseDir:
		{
			DbDirNode *dbDirNode = &fsObjName->variant.dbDirNode;

			MirroredFileSysObj_DropDbDir(
								dbDirNode,
								/* primaryOnly */ false,
								/* mirrorOnly */ true,
								/* ignoreNonExistence */ true,
								mirrorDataLossOccurred);
			if (*mirrorDataLossOccurred)
				return;
		}
		break;

	case PersistentFsObjType_TablespaceDir:
		MirroredFileSysObj_DropTablespaceDir(
							fsObjName->variant.tablespaceOid,
							/* primaryOnly */ false,
							/* mirrorOnly */ true,
							/* ignoreNonExistence */ true,
							mirrorDataLossOccurred);

		if (*mirrorDataLossOccurred)
			return;

		break;

	case PersistentFsObjType_FilespaceDir:
		{
			char *primaryFilespaceLocation = NULL;
			char *mirrorFilespaceLocation = NULL;

			PersistentFilespace_GetPrimaryAndMirror(
								fsObjName->variant.filespaceOid,
								&primaryFilespaceLocation,
								&mirrorFilespaceLocation);

			MirroredFileSysObj_DropFilespaceDir(
								fsObjName->variant.filespaceOid,
								primaryFilespaceLocation,
								mirrorFilespaceLocation,
								/* primaryOnly */ false,
								/* mirrorOnly */ true,
								/* ignoreNonExistence */ true,
								mirrorDataLossOccurred);

			if (*mirrorDataLossOccurred)
				return;

			if (primaryFilespaceLocation != NULL)
				pfree(primaryFilespaceLocation);
			if (mirrorFilespaceLocation != NULL)
				pfree(mirrorFilespaceLocation);
		}
		break;

	default:
		elog(ERROR, "Unexpected persistent file-system object type: %d",
			 fsObjName->type);
	}
}

/*
 * Performs updates to persistent table entries to mark them as dropped (i.e. free).
 * This function (currently) is used during resync transition phase.
 *
 * Input parameters help this function to determine exact database object for drop
 * task or to find the exact persistent table tuple to mark it dropped/free.
 */
static void PersistentFileSysObj_DoMirrorReDropped(
	PersistentFileSysObjName 	*fsObjName,
	PersistentFileSysRelStorageMgr relStorageMgr,
	ItemPointer					persistentTid,
	int64						persistentSerialNum)
{
	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent '%s' tuple for mirror re-drop is invalid (0,0)",
			 PersistentFileSysObjName_TypeName(fsObjName->type));

	if (persistentSerialNum == 0)
		elog(ERROR, "Persistent '%s' serial number for mirror re-drop is invalid (0)",
			 PersistentFileSysObjName_TypeName(fsObjName->type));

	switch (fsObjName->type)
	{
		case PersistentFsObjType_RelationFile:
		{
			RelFileNode *relFileNode = PersistentFileSysObjName_GetRelFileNodePtr(fsObjName);
			int32 segmentFileNum = PersistentFileSysObjName_GetSegmentFileNum(fsObjName);

			Assert(relStorageMgr != PersistentFileSysRelStorageMgr_None);

			PersistentRelation_Dropped(
								relFileNode,
								segmentFileNum,
								persistentTid,
								persistentSerialNum);
		}
		break;

		case PersistentFsObjType_DatabaseDir:
		{
			DbDirNode *dbDirNode = &fsObjName->variant.dbDirNode;

			PersistentDatabase_Dropped(
								dbDirNode,
								persistentTid,
								persistentSerialNum);
		}
		break;

		case PersistentFsObjType_TablespaceDir:
			PersistentTablespace_Dropped(
								fsObjName->variant.tablespaceOid,
								persistentTid,
								persistentSerialNum);
			break;

		case PersistentFsObjType_FilespaceDir:
			PersistentFilespace_Dropped(
								fsObjName->variant.filespaceOid,
								persistentTid,
								persistentSerialNum);
			break;

		default:
			elog(ERROR, "Unexpected persistent file-system object type: %d",
				 fsObjName->type);
	}
}

void PersistentFileSysObj_EndXactDrop(
	PersistentFileSysObjName 	*fsObjName,
	PersistentFileSysRelStorageMgr relStorageMgr,
	char						*relationName,
					/* For tracing only.  Can be NULL in some execution paths. */
	ItemPointer				persistentTid,
	int64					persistentSerialNum,
	bool					ignoreNonExistence)
{
	// NOTE: The caller must already have the MirroredLock.

	if (fsObjName->type == PersistentFsObjType_RelationFile)
	{
		/*
		 * We use this lock to guard data resynchronization.
		 */
		LockRelationForResynchronize(
						PersistentFileSysObjName_GetRelFileNodePtr(fsObjName),
						AccessExclusiveLock);
	}

	PersistentFileSysObj_DropObject(
							fsObjName,
							relStorageMgr,
							relationName,
							persistentTid,
							persistentSerialNum,
							ignoreNonExistence,
							Debug_persistent_print,
							Persistent_DebugPrintLevel());

	if (fsObjName->type == PersistentFsObjType_RelationFile)
	{
		UnlockRelationForResynchronize(
						PersistentFileSysObjName_GetRelFileNodePtr(fsObjName),
						AccessExclusiveLock);
	}
}

static void PersistentFileSysObj_UpdateTupleAppendOnlyMirrorResyncEofs(
	PersistentFileSysObjData 		*fileSysObjData,
	PersistentFileSysObjSharedData 	*fileSysObjSharedData,
	HeapTuple					tuple,
	ItemPointer 				persistentTid,
	int64						persistentSerialNum,
	int64						mirrorLossEof,
	int64						mirrorNewEof,
	bool						flushToXLog)
{
	Datum newValues[Natts_gp_persistent_relation_node];
	bool replaces[Natts_gp_persistent_relation_node];

	MemSet(newValues, 0, Natts_gp_persistent_relation_node * sizeof(Datum));
	MemSet(replaces, false, Natts_gp_persistent_relation_node * sizeof(bool));

	replaces[Anum_gp_persistent_relation_node_mirror_append_only_loss_eof - 1] = true;
	newValues[Anum_gp_persistent_relation_node_mirror_append_only_loss_eof - 1] =
													Int64GetDatum(mirrorLossEof);

	replaces[Anum_gp_persistent_relation_node_mirror_append_only_new_eof - 1] = true;
	newValues[Anum_gp_persistent_relation_node_mirror_append_only_new_eof - 1] =
													Int64GetDatum(mirrorNewEof);

	PersistentFileSysObj_ReplaceTuple(
								PersistentFsObjType_RelationFile,
								persistentTid,
								tuple,
								newValues,
								replaces,
								flushToXLog);
}

void PersistentFileSysObj_UpdateAppendOnlyMirrorResyncEofs(
	RelFileNode					*relFileNode,
	int32						segmentFileNum,
	ItemPointer 				persistentTid,
	int64						persistentSerialNum,
	bool						mirrorCatchupRequired,
	int64						mirrorNewEof,
	bool						recovery,
	bool						flushToXLog)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	READTUPLE_FOR_UPDATE_ERRCONTEXT_DECLARE;

	PersistentFileSysObjData		*fileSysObjData;
	PersistentFileSysObjSharedData	*fileSysObjSharedData;

	Datum values[Natts_gp_persistent_relation_node];

	HeapTuple tupleCopy;

	RelFileNode 					actualRelFileNode;
	int32							actualSegmentFileNum;

	PersistentFileSysRelStorageMgr	relationStorageManager;
	PersistentFileSysState			persistentState;
	int64							createMirrorDataLossTrackingSessionNum;
	MirroredObjectExistenceState	mirrorExistenceState;
	MirroredRelDataSynchronizationState mirrorDataSynchronizationState;
	bool							mirrorBufpoolMarkedForScanIncrementalResync;
	int64							mirrorBufpoolResyncChangedPageCount;
	XLogRecPtr						mirrorBufpoolResyncCkptLoc;
	BlockNumber 					mirrorBufpoolResyncCkptBlockNum;
	int64							mirrorAppendOnlyLossEof;
	int64							mirrorAppendOnlyNewEof;
	PersistentFileSysRelBufpoolKind relBufpoolKind;
	TransactionId					parentXid;
	int64							actualPersistentSerialNum;
	int64							updateMirrorAppendOnlyLossEof;

	PersistentFileSysObj_GetDataPtrs(
								PersistentFsObjType_RelationFile,
								&fileSysObjData,
								&fileSysObjSharedData);

	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent %u/%u/%u, segment file #%d, tuple for update Append-Only mirror resync EOFs is invalid (0,0)",
			 relFileNode->spcNode,
			 relFileNode->dbNode,
			 relFileNode->relNode,
			 segmentFileNum);

	if (persistentSerialNum == 0)
		elog(ERROR, "Persistent %u/%u/%u, segment file #%d, serial number for update Append-Only mirror resync EOFs is invalid (0)",
		relFileNode->spcNode,
		relFileNode->dbNode,
		relFileNode->relNode,
		segmentFileNum);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	READTUPLE_FOR_UPDATE_ERRCONTEXT_PUSHTYPE(PersistentFsObjType_RelationFile, persistentTid, persistentSerialNum);

	PersistentFileSysObj_ReadTuple(
							PersistentFsObjType_RelationFile,
							persistentTid,
							values,
							&tupleCopy);

	READTUPLE_FOR_UPDATE_ERRCONTEXT_POP;

	if (tupleCopy == NULL)
		elog(ERROR, "Persistent %u/%u/%u, segment file #%d, tuple at TID %s not found for update Append-Only mirror resync EOFs",
			 relFileNode->spcNode,
			 relFileNode->dbNode,
			 relFileNode->relNode,
			 segmentFileNum,
			 ItemPointerToString(persistentTid));

	GpPersistentRelationNode_GetValues(
									values,
									&actualRelFileNode.spcNode,
									&actualRelFileNode.dbNode,
									&actualRelFileNode.relNode,
									&actualSegmentFileNum,
									&relationStorageManager,
									&persistentState,
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
									&actualPersistentSerialNum);

	if (persistentSerialNum != actualPersistentSerialNum)
		elog(ERROR, "Persistent %u/%u/%u, segment file #%d, serial number mismatch for update Append-Only mirror resync EOFs (expected " INT64_FORMAT ", found " INT64_FORMAT "), at TID %s",
			 relFileNode->spcNode,
			 relFileNode->dbNode,
			 relFileNode->relNode,
			 segmentFileNum,
			 persistentSerialNum,
			 actualPersistentSerialNum,
			 ItemPointerToString(persistentTid));

	if (relationStorageManager != PersistentFileSysRelStorageMgr_AppendOnly)
		elog(ERROR, "Persistent %u/%u/%u, segment file #%d, relation storage manager mismatch for update Append-Only mirror resync EOFs (expected '%s', found '%s'), at TID %s",
			 relFileNode->spcNode,
			 relFileNode->dbNode,
			 relFileNode->relNode,
			 segmentFileNum,
			 PersistentFileSysRelStorageMgr_Name(PersistentFileSysRelStorageMgr_AppendOnly),
			 PersistentFileSysRelStorageMgr_Name(relationStorageManager),
			 ItemPointerToString(persistentTid));

	if (!recovery)
	{
		if (mirrorAppendOnlyNewEof >= mirrorNewEof)
			elog(ERROR, "Persistent %u/%u/%u, segment file #%d, current new EOF is greater than or equal to update new EOF for Append-Only mirror resync EOFs (current new EOF " INT64_FORMAT ", update new EOF " INT64_FORMAT "), persistent serial num " INT64_FORMAT " at TID %s",
				 relFileNode->spcNode,
				 relFileNode->dbNode,
				 relFileNode->relNode,
				 segmentFileNum,
				 mirrorAppendOnlyNewEof,
				 mirrorNewEof,
				 persistentSerialNum,
				 ItemPointerToString(persistentTid));
	}
	else
	{
		int level = ERROR;

		if (gp_crash_recovery_suppress_ao_eof)
		{
			level = WARNING;
		}

		if (mirrorAppendOnlyNewEof > mirrorNewEof)
		{
			elog(level,
				 "Persistent %u/%u/%u, segment file #%d, "
				 "current new EOF is greater than update new EOF for Append-Only mirror resync EOFs recovery "
				 "(current new EOF " INT64_FORMAT ", update new EOF " INT64_FORMAT "), persistent serial num " INT64_FORMAT " at TID %s",
				 relFileNode->spcNode,
				 relFileNode->dbNode,
				 relFileNode->relNode,
				 segmentFileNum,
				 mirrorAppendOnlyNewEof,
				 mirrorNewEof,
				 persistentSerialNum,
				 ItemPointerToString(persistentTid));

			WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

			return;
		}

		if (mirrorAppendOnlyNewEof == mirrorNewEof)
		{
			if (Debug_persistent_print ||
				Debug_persistent_appendonly_commit_count_print)
				elog(Persistent_DebugPrintLevel(),
					 "Persistent %u/%u/%u, segment file #%d, current new EOF equal to update new EOF for Append-Only mirror resync EOFs recovery (EOF " INT64_FORMAT "), persistent serial num " INT64_FORMAT " at TID %s",
					 relFileNode->spcNode,
					 relFileNode->dbNode,
					 relFileNode->relNode,
					 segmentFileNum,
					 mirrorAppendOnlyNewEof,
					 persistentSerialNum,
					 ItemPointerToString(persistentTid));

			WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

			return;		// Old news.
		}
	}

	if (mirrorCatchupRequired)
	{
		updateMirrorAppendOnlyLossEof = mirrorAppendOnlyLossEof;	// No change.
	}
	else
	{
		updateMirrorAppendOnlyLossEof = mirrorNewEof;				// No loss.
	}

	PersistentFileSysObj_UpdateTupleAppendOnlyMirrorResyncEofs(
												fileSysObjData,
												fileSysObjSharedData,
												tupleCopy,
												persistentTid,
												persistentSerialNum,
												updateMirrorAppendOnlyLossEof,
												mirrorNewEof,
												flushToXLog);

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print ||
		Debug_persistent_appendonly_commit_count_print)
		elog(Persistent_DebugPrintLevel(),
			 "PersistentFileSysObj_UpdateAppendOnlyMirrorResyncEofs: %u/%u/%u, segment file #%d, Append-Only Mirror Resync EOFs (recovery %s) -- persistent serial num " INT64_FORMAT ", TID %s, "
			 "mirror catchup required %s"
			 ", original mirror loss EOF " INT64_FORMAT ", update mirror loss EOF " INT64_FORMAT
			 ", original mirror new EOF " INT64_FORMAT ", update mirror new EOF " INT64_FORMAT,
			 relFileNode->spcNode,
			 relFileNode->dbNode,
			 relFileNode->relNode,
			 segmentFileNum,
			 (recovery ? "true" : "false"),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 (mirrorCatchupRequired ? "true" : "false"),
			 mirrorAppendOnlyLossEof,
			 updateMirrorAppendOnlyLossEof,
			 mirrorAppendOnlyNewEof,
			 mirrorNewEof);
}

void PersistentFileSysObj_PreparedEndXactAction(
	TransactionId 					preparedXid,
	const char 						*gid,
	PersistentEndXactRecObjects 	*persistentObjects,
	bool							isCommit,
	int								prepareAppendOnlyIntentCount)
{
	MIRRORED_LOCK_DECLARE;

	PersistentFileSysObjStateChangeResult *stateChangeResults;

	int i;

	if (Debug_persistent_print ||
		Debug_persistent_appendonly_commit_count_print)
		elog(Persistent_DebugPrintLevel(),
			 "PersistentFileSysObj_PreparedEndXactAction (distributed transaction id %s, local prepared xid %u): commit %s, file-system action count %d, shared-memory Append-Only intent count %d, xlog Append-Only mirror resync EOFs count %d",
			 gid,
			 preparedXid,
			 (isCommit ? "true" : "false"),
			 persistentObjects->typed.fileSysActionInfosCount,
			 prepareAppendOnlyIntentCount,
			 persistentObjects->typed.appendOnlyMirrorResyncEofsCount);

	stateChangeResults =
			(PersistentFileSysObjStateChangeResult*)
					palloc0(persistentObjects->typed.fileSysActionInfosCount * sizeof(PersistentFileSysObjStateChangeResult));

	/*
	 * We need to do the transition to 'Aborting Create' or 'Drop Pending' and perform
	 * the file-system drop while under one acquistion of the MirroredLock.  Otherwise,
	 * we could race with resynchronize's ReDrop.
	 */
	MIRRORED_LOCK;

	/*
	 * We need to complete this work, or let Crash Recovery complete it.
	 */
	START_CRIT_SECTION();

	/*
	 * End transaction State-Changes.
	 */
	for (i = 0; i < persistentObjects->typed.fileSysActionInfosCount; i++)
	{
		PersistentEndXactFileSysActionInfo	*fileSysActionInfo =
					&persistentObjects->typed.fileSysActionInfos[i];

		PersistentEndXactFileSysAction action;

		action = fileSysActionInfo->action;

		switch (action)
		{
		case PersistentEndXactFileSysAction_Create:
			if (isCommit)
			{
				/*
				 * 'Create Pending' --> 'Created'.
				 */
				if (Debug_persistent_print)
					elog(Persistent_DebugPrintLevel(),
						 "PersistentFileSysObj_PreparedEndXactAction (distributed transaction id %s, local prepared xid %u): [%d] %s, persistent serial num " INT64_FORMAT ", TID %s, 'Create Pending' --> 'Created'",
						 gid,
						 preparedXid,
						 i,
						 PersistentFileSysObjName_TypeAndObjectName(&fileSysActionInfo->fsObjName),
						 fileSysActionInfo->persistentSerialNum,
						 ItemPointerToString(&fileSysActionInfo->persistentTid));

				PersistentFileSysObj_Created(
										&fileSysActionInfo->fsObjName,
										&fileSysActionInfo->persistentTid,
										fileSysActionInfo->persistentSerialNum,
										/* retryPossible */ true);
			}
			else
			{
				/*
				 * 'Create Pending' --> 'Aborting Create'.
				 */
				if (Debug_persistent_print)
					elog(Persistent_DebugPrintLevel(),
						 "PersistentFileSysObj_PreparedEndXactAction (distributed transaction id %s, local prepared xid %u): [%d] %s, persistent serial num " INT64_FORMAT ", TID %s, 'Create Pending' --> 'Aborting Create'",
						 gid,
						 preparedXid,
						 i,
						 PersistentFileSysObjName_TypeAndObjectName(&fileSysActionInfo->fsObjName),
						 fileSysActionInfo->persistentSerialNum,
						 ItemPointerToString(&fileSysActionInfo->persistentTid));

				stateChangeResults[i] =
					PersistentFileSysObj_MarkAbortingCreate(
											&fileSysActionInfo->fsObjName,
											&fileSysActionInfo->persistentTid,
											fileSysActionInfo->persistentSerialNum,
											/* retryPossible */ true);
			}
			break;

		case PersistentEndXactFileSysAction_Drop:
			if (isCommit)
			{
				/*
				 * 'Created' --> 'Drop Pending'.
				 */
				if (Debug_persistent_print)
					elog(Persistent_DebugPrintLevel(),
						 "PersistentFileSysObj_PreparedEndXactAction (distributed transaction id %s, local prepared xid %u): [%d] %s, persistent serial num " INT64_FORMAT ", TID %s, 'Created' --> 'Drop Pending'",
						 gid,
						 preparedXid,
						 i,
						 PersistentFileSysObjName_TypeAndObjectName(&fileSysActionInfo->fsObjName),
						 fileSysActionInfo->persistentSerialNum,
						 ItemPointerToString(&fileSysActionInfo->persistentTid));

				stateChangeResults[i] =
					PersistentFileSysObj_MarkDropPending(
										&fileSysActionInfo->fsObjName,
										&fileSysActionInfo->persistentTid,
										fileSysActionInfo->persistentSerialNum,
										/* retryPossible */ true);
			}
			break;

		case PersistentEndXactFileSysAction_AbortingCreateNeeded:
			if (Debug_persistent_print)
				elog(Persistent_DebugPrintLevel(),
					 "PersistentFileSysObj_PreparedEndXactAction (distributed transaction id %s, local prepared xid %u): [%d] %s, persistent serial num " INT64_FORMAT ", TID %s, 'Create Pending' --> 'Aborting Create'",
					 gid,
					 preparedXid,
					 i,
					 PersistentFileSysObjName_TypeAndObjectName(&fileSysActionInfo->fsObjName),
					 fileSysActionInfo->persistentSerialNum,
					 ItemPointerToString(&fileSysActionInfo->persistentTid));

			stateChangeResults[i] =
				PersistentFileSysObj_MarkAbortingCreate(
									&fileSysActionInfo->fsObjName,
									&fileSysActionInfo->persistentTid,
									fileSysActionInfo->persistentSerialNum,
									/* retryPossible */ true);
			break;

		default:
			elog(ERROR, "Unexpected persistent end transaction file-system action: %d",
				 action);
		}
	}

	/*
	 * Make the above State-Changes permanent.
	 */
	PersistentFileSysObj_FlushXLog();

	/*
	 * Do the physically drop of the file-system objects.
	 */
	for (i = 0; i < persistentObjects->typed.fileSysActionInfosCount; i++)
	{
		PersistentEndXactFileSysActionInfo	*fileSysActionInfo =
					&persistentObjects->typed.fileSysActionInfos[i];

		PersistentEndXactFileSysAction action;

		bool dropPending;
		bool abortingCreate;

		action = fileSysActionInfo->action;

		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "PersistentFileSysObj_PreparedEndXactAction (distributed transaction id %s, local prepared xid %u): [%d] %s, persistent serial num " INT64_FORMAT ", TID %s, "
				 "looking at persistent end transaction action '%s' where isCommit %s and State-Change result '%s'",
				 gid,
				 preparedXid,
				 i,
				 PersistentFileSysObjName_TypeAndObjectName(&fileSysActionInfo->fsObjName),
				 fileSysActionInfo->persistentSerialNum,
				 ItemPointerToString(&fileSysActionInfo->persistentTid),
				 PersistentEndXactFileSysAction_Name(action),
				 (isCommit ? "true" : "false"),
				 PersistentFileSysObjStateChangeResult_Name(stateChangeResults[i]));

		dropPending = false;		// Assume.
		abortingCreate = false;		// Assume.

		switch (action)
		{
		case PersistentEndXactFileSysAction_Create:
			if (!isCommit)
			{
				abortingCreate = true;
			}
#ifdef FAULT_INJECTOR
				if (FaultInjector_InjectFaultIfSet(
											   isCommit ?
											   FinishPreparedTransactionCommitPass1FromCreatePendingToCreated :
											   FinishPreparedTransactionAbortPass1FromCreatePendingToAbortingCreate,
											   DDLNotSpecified,
												   "" /* databaseName */,
												   "" /* tableName */) == TRUE)
					goto injectfaultexit;  // skip pass2
#endif
			break;

		case PersistentEndXactFileSysAction_Drop:
			if (isCommit)
			{
				dropPending = true;
#ifdef FAULT_INJECTOR
				if (FaultInjector_InjectFaultIfSet(
											   FinishPreparedTransactionCommitPass1FromDropInMemoryToDropPending,
											   DDLNotSpecified,
											   "" /* databaseName */,
											   "" /* tableName */) == TRUE)
				goto injectfaultexit;  // skip pass2
#endif
			}
			break;

		case PersistentEndXactFileSysAction_AbortingCreateNeeded:
			abortingCreate = true;
#ifdef FAULT_INJECTOR
				if (FaultInjector_InjectFaultIfSet(
											   isCommit ?
											   FinishPreparedTransactionCommitPass1AbortingCreateNeeded:
											   FinishPreparedTransactionAbortPass1AbortingCreateNeeded,
											   DDLNotSpecified,
											   "" /* databaseName */,
											   "" /* tableName */) == TRUE)
				goto injectfaultexit;  // skip pass2
#endif
			break;

		default:
			elog(ERROR, "Unexpected persistent end transaction file-system action: %d",
				 action);
		}

		if (abortingCreate || dropPending)
		{
			if (stateChangeResults[i] == PersistentFileSysObjStateChangeResult_StateChangeOk)
			{
				if (Debug_persistent_print)
					elog(Persistent_DebugPrintLevel(),
						 "PersistentFileSysObj_PreparedEndXactAction (distributed transaction id %s, local prepared xid %u): [%d] %s, persistent serial num " INT64_FORMAT ", TID %s, "
						 "going to drop object where abortingCreate %s or dropPending %s",
						 gid,
						 preparedXid,
						 i,
						 PersistentFileSysObjName_TypeAndObjectName(&fileSysActionInfo->fsObjName),
						 fileSysActionInfo->persistentSerialNum,
						 ItemPointerToString(&fileSysActionInfo->persistentTid),
						 (abortingCreate ? "true" : "false"),
						 (dropPending ? "true" : "false"));

				PersistentFileSysObj_EndXactDrop(
										&fileSysActionInfo->fsObjName,
										fileSysActionInfo->relStorageMgr,
										/* relationName */ NULL,		// Ok to be NULL -- we don't know the name here.
										&fileSysActionInfo->persistentTid,
										fileSysActionInfo->persistentSerialNum,
										/* ignoreNonExistence */ abortingCreate);
			}
			else
			{
				if (Debug_persistent_print)
					elog(Persistent_DebugPrintLevel(),
						 "PersistentFileSysObj_PreparedEndXactAction (distributed transaction id %s, local prepared xid %u): [%d] %s, persistent serial num " INT64_FORMAT ", TID %s, "
						 "skipping to drop object where abortingCreate %s or dropPending %s with State-Change result '%s'",
						 gid,
						 preparedXid,
						 i,
						 PersistentFileSysObjName_TypeAndObjectName(&fileSysActionInfo->fsObjName),
						 fileSysActionInfo->persistentSerialNum,
						 ItemPointerToString(&fileSysActionInfo->persistentTid),
						 (abortingCreate ? "true" : "false"),
						 (dropPending ? "true" : "false"),
						 PersistentFileSysObjStateChangeResult_Name(stateChangeResults[i]));
			}
		}

#ifdef FAULT_INJECTOR
		switch (action)
		{
			case PersistentEndXactFileSysAction_Create:
				if (FaultInjector_InjectFaultIfSet(
												   isCommit ?
												   FinishPreparedTransactionCommitPass2FromCreatePendingToCreated :
												   FinishPreparedTransactionAbortPass2FromCreatePendingToAbortingCreate,
												   DDLNotSpecified,
												   "" /* databaseName */,
												   "" /* tableName */) == TRUE)
					goto injectfaultexit;  // skip physical drop in pass2

				break;

			case PersistentEndXactFileSysAction_Drop:
				if (isCommit)
				{
					if (FaultInjector_InjectFaultIfSet(
												   FinishPreparedTransactionCommitPass2FromDropInMemoryToDropPending,
												   DDLNotSpecified,
												   "" /* databaseName */,
												   "" /* tableName */) == TRUE)
					goto injectfaultexit;  // skip physical drop in pass2
				}
				break;

			case PersistentEndXactFileSysAction_AbortingCreateNeeded:
				if (FaultInjector_InjectFaultIfSet(
											   isCommit ?
											   FinishPreparedTransactionCommitPass2AbortingCreateNeeded :
											   FinishPreparedTransactionAbortPass2AbortingCreateNeeded,
											   DDLNotSpecified,
											   "" /* databaseName */,
											   "" /* tableName */) == TRUE)
				goto injectfaultexit;  // skip physical drop in pass2
				break;

			default:
				break;
		}

#endif
	}

	if (isCommit)
	{
		/*
		 * Append-Only mirror resync EOFs.
		 */
		for (i = 0; i < persistentObjects->typed.appendOnlyMirrorResyncEofsCount; i++)
		{
			bool mirrorCatchupRequired;

			PersistentEndXactAppendOnlyMirrorResyncEofs *eofs =
						&persistentObjects->typed.appendOnlyMirrorResyncEofs[i];

			if (eofs->mirrorLossEof == INT64CONST(-1))
			{
				mirrorCatchupRequired = true;
			}
			else
			{
				if (eofs->mirrorLossEof != eofs->mirrorNewEof)
					elog(ERROR, "mirror loss EOF " INT64_FORMAT " doesn't match new EOF " INT64_FORMAT,
						 eofs->mirrorLossEof,
						 eofs->mirrorNewEof);
				mirrorCatchupRequired = false;
			}

			SIMPLE_FAULT_INJECTOR(UpdateCommittedEofInPersistentTable);

			PersistentFileSysObj_UpdateAppendOnlyMirrorResyncEofs(
															&eofs->relFileNode,
															eofs->segmentFileNum,
															&eofs->persistentTid,
															eofs->persistentSerialNum,
															mirrorCatchupRequired,
															eofs->mirrorNewEof,
															/* recovery */ false,
															/* flushToXLog */ false);
		}
	}

	/*
	 * For both COMMIT and ABORT, release our intent to add Append-Only mirror resync EOFs.
	 */
	if (persistentObjects->typed.appendOnlyMirrorResyncEofsCount > 0 &&
		prepareAppendOnlyIntentCount > 0)
	{
		PersistentEndXactAppendOnlyMirrorResyncEofs *eofsExample;

		int oldSystemAppendOnlyCommitWorkCount;
		int newSystemAppendOnlyCommitWorkCount;
		int resultSystemAppendOnlyCommitWorkCount;

		eofsExample = &persistentObjects->typed.appendOnlyMirrorResyncEofs[0];

		if (persistentObjects->typed.appendOnlyMirrorResyncEofsCount != prepareAppendOnlyIntentCount)
			elog(ERROR,
				 "Append-Only Mirror Resync EOFs intent count mismatch "
				 "(shared-memory count %d, xlog count %d). "
				 "Example relation %u/%u/%u, segment file #%d (persistent serial num " INT64_FORMAT ", TID %s).  "
				 "Distributed transaction id %s (local prepared xid %u)",
				 prepareAppendOnlyIntentCount,
				 persistentObjects->typed.appendOnlyMirrorResyncEofsCount,
				 eofsExample->relFileNode.spcNode,
				 eofsExample->relFileNode.dbNode,
				 eofsExample->relFileNode.relNode,
				 eofsExample->segmentFileNum,
				 eofsExample->persistentSerialNum,
				 ItemPointerToString(&eofsExample->persistentTid),
				 gid,
				 preparedXid);

		LWLockAcquire(FileRepAppendOnlyCommitCountLock , LW_EXCLUSIVE);

		oldSystemAppendOnlyCommitWorkCount = FileRepPrimary_GetAppendOnlyCommitWorkCount();

		newSystemAppendOnlyCommitWorkCount =
						oldSystemAppendOnlyCommitWorkCount -
						persistentObjects->typed.appendOnlyMirrorResyncEofsCount;

		if (newSystemAppendOnlyCommitWorkCount < 0)
			elog(ERROR,
				 "Append-Only Mirror Resync EOFs intent count would go negative "
				 "(enter system count %d, subtract count %d). "
				 "Example relation %u/%u/%u, segment file #%d (persistent serial num " INT64_FORMAT ", TID %s).  "
				 "Distributed transaction id %s (local prepared xid %u)",
				 oldSystemAppendOnlyCommitWorkCount,
				 persistentObjects->typed.appendOnlyMirrorResyncEofsCount,
				 eofsExample->relFileNode.spcNode,
				 eofsExample->relFileNode.dbNode,
				 eofsExample->relFileNode.relNode,
				 eofsExample->segmentFileNum,
				 eofsExample->persistentSerialNum,
				 ItemPointerToString(&eofsExample->persistentTid),
				 gid,
				 preparedXid);

		resultSystemAppendOnlyCommitWorkCount =
				FileRepPrimary_FinishedAppendOnlyCommitWork(
					persistentObjects->typed.appendOnlyMirrorResyncEofsCount);

		// Should match since we are under FileRepAppendOnlyCommitCountLock EXCLUSIVE.
		Assert(newSystemAppendOnlyCommitWorkCount == resultSystemAppendOnlyCommitWorkCount);

		if (Debug_persistent_print ||
			Debug_persistent_appendonly_commit_count_print)
			elog(Persistent_DebugPrintLevel(),
				 "PersistentFileSysObj_PreparedEndXactAction: Append-Only Mirror Resync EOFs finishing commit work "
				 "(enter system count %d, subtract count %d, result system count %d).  "
				 "Example relation %u/%u/%u, segment file #%d (persistent serial num " INT64_FORMAT ", TID %s)  "
				 "Distributed transaction id %s (local prepared xid %u)",
				 oldSystemAppendOnlyCommitWorkCount,
				 persistentObjects->typed.appendOnlyMirrorResyncEofsCount,
				 resultSystemAppendOnlyCommitWorkCount,
				 eofsExample->relFileNode.spcNode,
				 eofsExample->relFileNode.dbNode,
				 eofsExample->relFileNode.relNode,
				 eofsExample->segmentFileNum,
				 eofsExample->persistentSerialNum,
				 ItemPointerToString(&eofsExample->persistentTid),
				 gid,
				 preparedXid);

		LWLockRelease(FileRepAppendOnlyCommitCountLock);
	}
	else if (prepareAppendOnlyIntentCount > 0)
	{
		elog(ERROR,
			 "Append-Only Mirror Resync EOFs intent count in shared-memory non-zero (%d) and xlog count is zero. "
			 "Distributed transaction id %s (local prepared xid %u)",
			 prepareAppendOnlyIntentCount,
			 gid,
			 preparedXid);
	}

	goto jumpoverinjectfaultexit;

injectfaultexit:

	if (Debug_persistent_print ||
		Debug_persistent_appendonly_commit_count_print)
	{
		int systemAppendOnlyCommitWorkCount;

		LWLockAcquire(FileRepAppendOnlyCommitCountLock , LW_EXCLUSIVE);

		systemAppendOnlyCommitWorkCount = FileRepPrimary_GetAppendOnlyCommitWorkCount();

		elog(Persistent_DebugPrintLevel(),
			 "PersistentFileSysObj_PreparedEndXactAction: Append-Only Mirror Resync EOFs commit count %d at fault-injection "
			 "(shared-memory count %d, xlog count %d). "
			 "Distributed transaction id %s (local prepared xid %u)",
			 systemAppendOnlyCommitWorkCount,
			 prepareAppendOnlyIntentCount,
			 persistentObjects->typed.appendOnlyMirrorResyncEofsCount,
			 gid,
			 preparedXid);

		LWLockRelease(FileRepAppendOnlyCommitCountLock);
	}

jumpoverinjectfaultexit:

	PersistentFileSysObj_FlushXLog();

	MIRRORED_UNLOCK;

	END_CRIT_SECTION();

	pfree(stateChangeResults);
}


static void PersistentFileSysObj_UpdateTupleMirrorExistenceState(
	PersistentFileSysObjData 		*fileSysObjData,
	PersistentFileSysObjSharedData 	*fileSysObjSharedData,
	PersistentFsObjType				fsObjType,
	HeapTuple					tuple,
	ItemPointer 				persistentTid,
	int64						persistentSerialNum,
	MirroredObjectExistenceState mirrorExistenceState,
	bool						flushToXLog)
{
	Datum *newValues;
	bool *replaces;

	newValues = (Datum*)palloc0(fileSysObjData->storeData.numAttributes * sizeof(Datum));
	replaces = (bool*)palloc0(fileSysObjData->storeData.numAttributes * sizeof(bool));

	replaces[fileSysObjData->attNumMirrorExistenceState - 1] = true;
	newValues[fileSysObjData->attNumMirrorExistenceState - 1] = Int16GetDatum(mirrorExistenceState);

	PersistentFileSysObj_ReplaceTuple(
								fsObjType,
								persistentTid,
								tuple,
								newValues,
								replaces,
								flushToXLog);

	pfree(newValues);
	pfree(replaces);
}


static void PersistentFileSysObj_UpdateTupleMirrorState(
	PersistentFileSysObjData 		*fileSysObjData,
	PersistentFileSysObjSharedData 	*fileSysObjSharedData,
	PersistentFsObjType				fsObjType,
	HeapTuple					tuple,
	ItemPointer 				persistentTid,
	int64						persistentSerialNum,
	MirroredObjectExistenceState mirrorExistenceState,
	MirroredRelDataSynchronizationState relDataSynchronizationState,
	bool						flushToXLog)
{
	Datum *newValues;
	bool *replaces;

	newValues = (Datum*)palloc0(fileSysObjData->storeData.numAttributes * sizeof(Datum));
	replaces = (bool*)palloc0(fileSysObjData->storeData.numAttributes * sizeof(bool));

	replaces[fileSysObjData->attNumMirrorExistenceState - 1] = true;
	newValues[fileSysObjData->attNumMirrorExistenceState - 1] = Int16GetDatum(mirrorExistenceState);

	if (fsObjType == PersistentFsObjType_RelationFile)
	{
		replaces[Anum_gp_persistent_relation_node_mirror_data_synchronization_state - 1] = true;
		newValues[Anum_gp_persistent_relation_node_mirror_data_synchronization_state - 1] = Int16GetDatum(relDataSynchronizationState);
	}

	PersistentFileSysObj_ReplaceTuple(
								fsObjType,
								persistentTid,
								tuple,
								newValues,
								replaces,
								flushToXLog);

	pfree(newValues);
	pfree(replaces);
}

static void PersistentFileSysObj_UpdateTupleRelDataSynchronizationState(
	PersistentFileSysObjData 		*fileSysObjData,
	PersistentFileSysObjSharedData 	*fileSysObjSharedData,
	HeapTuple					tuple,
	ItemPointer 				persistentTid,
	int64						persistentSerialNum,
	MirroredRelDataSynchronizationState relDataSynchronizationState,
	bool						flushToXLog)
{
	Datum *newValues;
	bool *replaces;

	newValues = (Datum*)palloc0(Natts_gp_persistent_relation_node * sizeof(Datum));
	replaces = (bool*)palloc0(Natts_gp_persistent_relation_node * sizeof(bool));

	MemSet(newValues, 0, Natts_gp_persistent_relation_node * sizeof(Datum));
	MemSet(replaces, false, Natts_gp_persistent_relation_node * sizeof(bool));

	replaces[Anum_gp_persistent_relation_node_mirror_data_synchronization_state - 1] = true;
	newValues[Anum_gp_persistent_relation_node_mirror_data_synchronization_state - 1] = Int16GetDatum(relDataSynchronizationState);

	PersistentFileSysObj_ReplaceTuple(
								PersistentFsObjType_RelationFile,
								persistentTid,
								tuple,
								newValues,
								replaces,
								flushToXLog);

	pfree(newValues);
	pfree(replaces);
}


static void PersistentFileSysObj_UpdateTupleMirrorReCreated(
	PersistentFileSysObjData 		*fileSysObjData,
	PersistentFileSysObjSharedData 	*fileSysObjSharedData,
	PersistentFsObjType				fsObjType,
	HeapTuple						tuple,
	ItemPointer 					persistentTid,
	int64							persistentSerialNum,
	PersistentFileSysRelStorageMgr 	relStorageMgr,
	MirroredObjectExistenceState 	newMirrorExistenceState,
	bool							flushToXLog)
{
	Datum *newValues;
	bool *replaces;

	newValues = (Datum*)palloc0(fileSysObjData->storeData.numAttributes * sizeof(Datum));
	replaces = (bool*)palloc0(fileSysObjData->storeData.numAttributes * sizeof(bool));

	replaces[fileSysObjData->attNumMirrorExistenceState - 1] = true;
	newValues[fileSysObjData->attNumMirrorExistenceState - 1] =
												Int16GetDatum(newMirrorExistenceState);

	if (fsObjType == PersistentFsObjType_RelationFile)
	{
		replaces[Anum_gp_persistent_relation_node_mirror_data_synchronization_state - 1] = true;
		newValues[Anum_gp_persistent_relation_node_mirror_data_synchronization_state - 1] =
												Int16GetDatum(MirroredRelDataSynchronizationState_FullCopy);

		if (relStorageMgr == PersistentFileSysRelStorageMgr_AppendOnly)
		{
			replaces[Anum_gp_persistent_relation_node_mirror_append_only_loss_eof - 1] = true;
			newValues[Anum_gp_persistent_relation_node_mirror_append_only_loss_eof - 1] =
												Int64GetDatum(0);
		}
	}

	PersistentFileSysObj_ReplaceTuple(
								fsObjType,
								persistentTid,
								tuple,
								newValues,
								replaces,
								flushToXLog);

	pfree(newValues);
	pfree(replaces);
}

static void PersistentFileSysObj_UpdateTupleMarkBufPoolRelationForScanIncrementalResync(
	PersistentFileSysObjData 		*fileSysObjData,
	PersistentFileSysObjSharedData 	*fileSysObjSharedData,
	HeapTuple					tuple,
	ItemPointer 				persistentTid,
	int64						persistentSerialNum,
	bool						newMirrorBufpoolMarkedForScanIncrementalResync,
	bool						flushToXLog)
{
	Datum newValues[Natts_gp_persistent_relation_node];
	bool replaces[Natts_gp_persistent_relation_node];

	MemSet(newValues, 0, Natts_gp_persistent_relation_node * sizeof(Datum));
	MemSet(replaces, false, Natts_gp_persistent_relation_node * sizeof(bool));

	replaces[Anum_gp_persistent_relation_node_mirror_bufpool_marked_for_scan_incremental_resync - 1] = true;
	newValues[Anum_gp_persistent_relation_node_mirror_bufpool_marked_for_scan_incremental_resync - 1] =
													BoolGetDatum(newMirrorBufpoolMarkedForScanIncrementalResync);

	PersistentFileSysObj_ReplaceTuple(
								PersistentFsObjType_RelationFile,
								persistentTid,
								tuple,
								newValues,
								replaces,
								flushToXLog);
}

static void PersistentFileSysObj_UpdateTupleScanIncrementalAndResetFlag(
	PersistentFileSysObjData 		*fileSysObjData,
	PersistentFileSysObjSharedData 	*fileSysObjSharedData,
	HeapTuple					tuple,
	ItemPointer 				persistentTid,
	int64						persistentSerialNum,
	bool						flushToXLog)
{
	Datum newValues[Natts_gp_persistent_relation_node];
	bool replaces[Natts_gp_persistent_relation_node];

	MemSet(newValues, 0, Natts_gp_persistent_relation_node * sizeof(Datum));
	MemSet(replaces, false, Natts_gp_persistent_relation_node * sizeof(bool));

	replaces[Anum_gp_persistent_relation_node_mirror_data_synchronization_state - 1] = true;
	newValues[Anum_gp_persistent_relation_node_mirror_data_synchronization_state - 1] = Int16GetDatum(MirroredRelDataSynchronizationState_BufferPoolScanIncremental);

	replaces[Anum_gp_persistent_relation_node_mirror_bufpool_marked_for_scan_incremental_resync - 1] = true;
	newValues[Anum_gp_persistent_relation_node_mirror_bufpool_marked_for_scan_incremental_resync - 1] = BoolGetDatum(false);

	PersistentFileSysObj_ReplaceTuple(
								PersistentFsObjType_RelationFile,
								persistentTid,
								tuple,
								newValues,
								replaces,
								flushToXLog);
}

static void PersistentFileSysObj_UpdateTuplePageIncremental(
	PersistentFileSysObjData 		*fileSysObjData,
	PersistentFileSysObjSharedData 	*fileSysObjSharedData,
	HeapTuple					tuple,
	ItemPointer 				persistentTid,
	int64						persistentSerialNum,
	int64						mirrorBufpoolResyncChangedPageCount,
	bool						flushToXLog)
{
	Datum *newValues;
	bool *replaces;

	newValues = (Datum*)palloc0(fileSysObjData->storeData.numAttributes * sizeof(Datum));
	replaces = (bool*)palloc0(fileSysObjData->storeData.numAttributes * sizeof(bool));

	replaces[Anum_gp_persistent_relation_node_mirror_data_synchronization_state - 1] = true;
	newValues[Anum_gp_persistent_relation_node_mirror_data_synchronization_state - 1] =
													Int16GetDatum(MirroredRelDataSynchronizationState_BufferPoolPageIncremental);

	replaces[Anum_gp_persistent_relation_node_mirror_bufpool_resync_changed_page_count - 1] = true;
	newValues[Anum_gp_persistent_relation_node_mirror_bufpool_resync_changed_page_count - 1] =
													Int64GetDatum(mirrorBufpoolResyncChangedPageCount);

	PersistentFileSysObj_ReplaceTuple(
								PersistentFsObjType_RelationFile,
								persistentTid,
								tuple,
								newValues,
								replaces,
								flushToXLog);

	pfree(newValues);
	pfree(replaces);
}

void PersistentFileSysObj_ChangeMirrorState(
	PersistentFsObjType				fsObjType,
	ItemPointer 					persistentTid,
	int64							persistentSerialNum,
	MirroredObjectExistenceState 	mirrorExistenceState,
	MirroredRelDataSynchronizationState relDataSynchronizationState,
	bool							flushToXLog)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	READTUPLE_FOR_UPDATE_ERRCONTEXT_DECLARE;

	PersistentFileSysObjData 		*fileSysObjData;
	PersistentFileSysObjSharedData 	*fileSysObjSharedData;

	Datum *values;

	HeapTuple tupleCopy;

	PersistentFileSysObj_GetDataPtrs(
								fsObjType,
								&fileSysObjData,
								&fileSysObjSharedData);

	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent '%s' tuple for update mirror existence state is invalid (0,0)",
			 PersistentFileSysObjName_TypeName(fsObjType));

	if (persistentSerialNum == 0)
		elog(ERROR, "Persistent '%s' serial number for update mirror existence state is invalid (0)",
			 PersistentFileSysObjName_TypeName(fsObjType));

	values = (Datum*)palloc(fileSysObjData->storeData.numAttributes * sizeof(Datum));

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	READTUPLE_FOR_UPDATE_ERRCONTEXT_PUSHTYPE(fsObjType, persistentTid, persistentSerialNum);

	PersistentFileSysObj_ReadTuple(
							fsObjType,
							persistentTid,
							values,
							&tupleCopy);

	READTUPLE_FOR_UPDATE_ERRCONTEXT_POP;

	PersistentFileSysObj_UpdateTupleMirrorState(
											fileSysObjData,
											fileSysObjSharedData,
											fsObjType,
											tupleCopy,
											persistentTid,
											persistentSerialNum,
											mirrorExistenceState,
											relDataSynchronizationState,
											flushToXLog);

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	pfree(values);
}

void PersistentFileSysObj_MarkBufPoolRelationForScanIncrementalResync(
	PersistentFileSysObjName 	*fsObjName,
	ItemPointer					persistentTid,
	int64						persistentSerialNum,
	bool						flushToXLog)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	READTUPLE_FOR_UPDATE_ERRCONTEXT_DECLARE;

	PersistentFileSysObjData 		*fileSysObjData;
	PersistentFileSysObjSharedData 	*fileSysObjSharedData;

	Datum *values;

	HeapTuple tupleCopy;

	RelFileNode 					actualRelFileNode;
	int32							actualSegmentFileNum;

	PersistentFileSysRelStorageMgr	relationStorageManager;
	PersistentFileSysState			persistentState;
	int64							createMirrorDataLossTrackingSessionNum;
	MirroredObjectExistenceState	mirrorExistenceState;
	MirroredRelDataSynchronizationState mirrorDataSynchronizationState;
	bool							mirrorBufpoolMarkedForScanIncrementalResync;
	int64							mirrorBufpoolResyncChangedPageCount;
	XLogRecPtr						mirrorBufpoolResyncCkptLoc;
	BlockNumber 					mirrorBufpoolResyncCkptBlockNum;
	int64							mirrorAppendOnlyLossEof;
	int64							mirrorAppendOnlyNewEof;
	PersistentFileSysRelBufpoolKind relBufpoolKind;
	TransactionId					parentXid;
	int64							actualPersistentSerialNum;

	PersistentFileSysObj_GetDataPtrs(
								PersistentFsObjType_RelationFile,
								&fileSysObjData,
								&fileSysObjSharedData);

	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent '%s' tuple for mark Buffer Pool relation for 'Scan Incremental' resync is invalid (0,0)",
			 PersistentFileSysObjName_TypeAndObjectName(fsObjName));

	if (persistentSerialNum == 0)
		elog(ERROR, "Persistent '%s' serial number for mark Buffer Pool relation for 'Scan Incremental' resync is invalid (0)",
			 PersistentFileSysObjName_TypeAndObjectName(fsObjName));

	values = (Datum*)palloc(fileSysObjData->storeData.numAttributes * sizeof(Datum));

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	READTUPLE_FOR_UPDATE_ERRCONTEXT_PUSHNAME(fsObjName, persistentTid, persistentSerialNum);

	PersistentFileSysObj_ReadTuple(
							PersistentFsObjType_RelationFile,
							persistentTid,
							values,
							&tupleCopy);

	READTUPLE_FOR_UPDATE_ERRCONTEXT_POP;

	GpPersistentRelationNode_GetValues(
									values,
									&actualRelFileNode.spcNode,
									&actualRelFileNode.dbNode,
									&actualRelFileNode.relNode,
									&actualSegmentFileNum,
									&relationStorageManager,
									&persistentState,
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
									&actualPersistentSerialNum);

	if (persistentSerialNum != actualPersistentSerialNum)
		elog(ERROR, "Persistent '%s' serial number mismatch for update physically truncated (expected " INT64_FORMAT ", found " INT64_FORMAT ")",
			 PersistentFileSysObjName_TypeAndObjectName(fsObjName),
			 persistentSerialNum,
			 actualPersistentSerialNum);

	if (relationStorageManager != PersistentFileSysRelStorageMgr_BufferPool)
		elog(ERROR, "Persistent '%s' relation storage manager mismatch for update physically truncated (expected '%s', found '%s')",
			 PersistentFileSysObjName_TypeAndObjectName(fsObjName),
			 PersistentFileSysRelStorageMgr_Name(PersistentFileSysRelStorageMgr_BufferPool),
			 PersistentFileSysRelStorageMgr_Name(relationStorageManager));

	PersistentFileSysObj_UpdateTupleMarkBufPoolRelationForScanIncrementalResync(
											fileSysObjData,
											fileSysObjSharedData,
											tupleCopy,
											persistentTid,
											persistentSerialNum,
											/* newMirrorBufpoolMarkedForScanIncrementalResync */ true,
											flushToXLog);

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	pfree(values);
}

bool PersistentFileSysObj_CanAppendOnlyCatchupDuringResync(
	RelFileNode					*relFileNode,
	int32						segmentFileNum,
	ItemPointer 				persistentTid,
	int64						persistentSerialNum,
	int64						*eof)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	READTUPLE_FOR_UPDATE_ERRCONTEXT_DECLARE;

	PersistentFileSysObjData 		*fileSysObjData;
	PersistentFileSysObjSharedData 	*fileSysObjSharedData;

	PersistentFileSysObjName 		fsObjName;

	Datum *values;

	HeapTuple tupleCopy;

	RelFileNode 					actualRelFileNode;
	int32							actualSegmentFileNum;

	PersistentFileSysRelStorageMgr	relationStorageManager;
	PersistentFileSysState			persistentState;
	int64							createMirrorDataLossTrackingSessionNum;
	MirroredObjectExistenceState	mirrorExistenceState;
	MirroredRelDataSynchronizationState mirrorDataSynchronizationState;
	bool							mirrorBufpoolMarkedForScanIncrementalResync;
	int64							mirrorBufpoolResyncChangedPageCount;
	XLogRecPtr						mirrorBufpoolResyncCkptLoc;
	BlockNumber 					mirrorBufpoolResyncCkptBlockNum;
	int64							mirrorAppendOnlyLossEof;
	int64							mirrorAppendOnlyNewEof;
	PersistentFileSysRelBufpoolKind relBufpoolKind;
	TransactionId					parentXid;
	int64							actualPersistentSerialNum;

	PersistentFileSysObjName_SetRelationFile(
										&fsObjName,
										relFileNode,
										segmentFileNum);

	PersistentFileSysObj_GetDataPtrs(
								PersistentFsObjType_RelationFile,
								&fileSysObjData,
								&fileSysObjSharedData);

	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent '%s' tuple for update relation physically truncated is invalid (0,0)",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName));

	if (persistentSerialNum == 0)
		elog(ERROR, "Persistent '%s' serial number for update relation physically truncated is invalid (0)",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName));

	values = (Datum*)palloc(fileSysObjData->storeData.numAttributes * sizeof(Datum));

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	READTUPLE_FOR_UPDATE_ERRCONTEXT_PUSHNAME(&fsObjName, persistentTid, persistentSerialNum);

	PersistentFileSysObj_ReadTuple(
							PersistentFsObjType_RelationFile,
							persistentTid,
							values,
							&tupleCopy);

	READTUPLE_FOR_UPDATE_ERRCONTEXT_POP;

	GpPersistentRelationNode_GetValues(
									values,
									&actualRelFileNode.spcNode,
									&actualRelFileNode.dbNode,
									&actualRelFileNode.relNode,
									&actualSegmentFileNum,
									&relationStorageManager,
									&persistentState,
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
									&actualPersistentSerialNum);

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	pfree(values);

	heap_freetuple(tupleCopy);

	if (persistentSerialNum != actualPersistentSerialNum)
		elog(ERROR, "Persistent '%s' serial number mismatch for check for Append-Only catch-up (expected " INT64_FORMAT ", found " INT64_FORMAT ")",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName),
			 persistentSerialNum,
			 actualPersistentSerialNum);

	if (relationStorageManager != PersistentFileSysRelStorageMgr_AppendOnly)
		elog(ERROR, "Persistent '%s' relation storage manager mismatch for check for Append-Only catch-up (expected '%s', found '%s')",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName),
			 PersistentFileSysRelStorageMgr_Name(PersistentFileSysRelStorageMgr_AppendOnly),
			 PersistentFileSysRelStorageMgr_Name(relationStorageManager));

	if (mirrorAppendOnlyLossEof < mirrorAppendOnlyNewEof)
	{
		/*
		 * Resynchronize needs to catch-up the mirror.
		 */
		return false;
	}
	Assert(mirrorAppendOnlyLossEof == mirrorAppendOnlyNewEof);

	if ((persistentState == PersistentFileSysState_CreatePending ||
		 persistentState == PersistentFileSysState_Created)
		&&
		mirrorDataSynchronizationState == MirroredRelDataSynchronizationState_FullCopy)
	{
		/*
		 * When Resynchronize need to do a full copy, we need to not access the mirror.
		 */
		return false;
	}

	*eof = mirrorAppendOnlyNewEof;
	return true;
}

void PersistentFileSysObj_GetAppendOnlyCatchupMirrorStartEof(
	RelFileNode					*relFileNode,
	int32						segmentFileNum,
	ItemPointer 				persistentTid,
	int64						persistentSerialNum,
	int64						*startEof)
{
	READ_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	READTUPLE_FOR_UPDATE_ERRCONTEXT_DECLARE;

	PersistentFileSysObjData 		*fileSysObjData;
	PersistentFileSysObjSharedData 	*fileSysObjSharedData;

	PersistentFileSysObjName 		fsObjName;

	Datum *values;

	HeapTuple tupleCopy;

	RelFileNode 					actualRelFileNode;
	int32							actualSegmentFileNum;

	PersistentFileSysRelStorageMgr	relationStorageManager;
	PersistentFileSysState			persistentState;
	int64							createMirrorDataLossTrackingSessionNum;
	MirroredObjectExistenceState	mirrorExistenceState;
	MirroredRelDataSynchronizationState mirrorDataSynchronizationState;
	bool							mirrorBufpoolMarkedForScanIncrementalResync;
	int64							mirrorBufpoolResyncChangedPageCount;
	XLogRecPtr						mirrorBufpoolResyncCkptLoc;
	BlockNumber 					mirrorBufpoolResyncCkptBlockNum;
	int64							mirrorAppendOnlyLossEof;
	int64							mirrorAppendOnlyNewEof;
	PersistentFileSysRelBufpoolKind relBufpoolKind;
	TransactionId					parentXid;
	int64							actualPersistentSerialNum;

	PersistentFileSysObjName_SetRelationFile(
										&fsObjName,
										relFileNode,
										segmentFileNum);

	PersistentFileSysObj_GetDataPtrs(
								PersistentFsObjType_RelationFile,
								&fileSysObjData,
								&fileSysObjSharedData);

	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent '%s' tuple for update relation physically truncated is invalid (0,0)",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName));

	if (persistentSerialNum == 0)
		elog(ERROR, "Persistent '%s' serial number for update relation physically truncated is invalid (0)",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName));

	values = (Datum*)palloc(fileSysObjData->storeData.numAttributes * sizeof(Datum));

	READ_PERSISTENT_STATE_ORDERED_LOCK;

	READTUPLE_FOR_UPDATE_ERRCONTEXT_PUSHNAME(&fsObjName, persistentTid, persistentSerialNum);

	PersistentFileSysObj_ReadTuple(
							PersistentFsObjType_RelationFile,
							persistentTid,
							values,
							&tupleCopy);

	READTUPLE_FOR_UPDATE_ERRCONTEXT_POP;

	GpPersistentRelationNode_GetValues(
									values,
									&actualRelFileNode.spcNode,
									&actualRelFileNode.dbNode,
									&actualRelFileNode.relNode,
									&actualSegmentFileNum,
									&relationStorageManager,
									&persistentState,
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
									&actualPersistentSerialNum);

	READ_PERSISTENT_STATE_ORDERED_UNLOCK;

	pfree(values);

	heap_freetuple(tupleCopy);

	if (persistentSerialNum != actualPersistentSerialNum)
		elog(ERROR, "Persistent '%s' serial number mismatch for check for Append-Only catch-up (expected " INT64_FORMAT ", found " INT64_FORMAT ")",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName),
			 persistentSerialNum,
			 actualPersistentSerialNum);

	if (relationStorageManager != PersistentFileSysRelStorageMgr_AppendOnly)
		elog(ERROR, "Persistent '%s' relation storage manager mismatch for check for Append-Only catch-up (expected '%s', found '%s')",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName),
			 PersistentFileSysRelStorageMgr_Name(PersistentFileSysRelStorageMgr_AppendOnly),
			 PersistentFileSysRelStorageMgr_Name(relationStorageManager));

	if (mirrorAppendOnlyLossEof != mirrorAppendOnlyNewEof)
		elog(ERROR, "Persistent '%s' EOF mismatch for check for Append-Only catch-up (loss EOF " INT64_FORMAT ", new EOF " INT64_FORMAT ")",
			 PersistentFileSysObjName_TypeAndObjectName(&fsObjName),
			 mirrorAppendOnlyLossEof,
			 mirrorAppendOnlyNewEof);

	*startEof = mirrorAppendOnlyNewEof;
}

static int32 PersistentFileSysObj_GetBufferPoolRelationTotalBlocks(
	RelFileNode			*relFileNode)
{
	SMgrRelation reln;

	int32 numOf32kBlocks;

	reln = smgropen(*relFileNode);

	numOf32kBlocks = smgrnblocks(reln, MAIN_FORKNUM);

	smgrclose(reln);

	return numOf32kBlocks;
}

typedef enum StateAction
{
	StateAction_None = 0,
	StateAction_MarkWholeMirrorFullCopy = 1,
	StateAction_MirrorReCreate = 2,
	StateAction_MarkMirrorReCreated = 3,
	StateAction_MirrorReDrop = 4,
	StateAction_MirrorReDropped = 5,
	StateAction_MirrorAdd = 6,
	MaxStateAction /* must always be last */
} StateAction;

static void PersistentFileSysObj_ScanStateAction(
	StateAction				stateAction,
	PersistentFsObjType		fsObjType)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjData *fileSysObjData;
	PersistentFileSysObjSharedData	*fileSysObjSharedData;

	PersistentStoreScan storeScan;

	HeapTuple tupleCopy;

	Datum *values;

	ItemPointerData persistentTid;
	int64 persistentSerialNum;

	PersistentFileSysState  		state;
	PersistentFileSysObjName		fsObjName;
	MirroredObjectExistenceState	mirrorExistenceState;
	TransactionId				 	parentXid;
	int64						 	serialNum;

	tupleCopy = NULL;

	PersistentFileSysObj_GetDataPtrs(
								fsObjType,
								&fileSysObjData,
								&fileSysObjSharedData);

	values = (Datum*)palloc(fileSysObjData->storeData.numAttributes * sizeof(Datum));

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

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
		GpPersistent_GetCommonValues(
								fsObjType,
								values,
								&fsObjName,
								&state,
								&mirrorExistenceState,
								&parentXid,
								&serialNum);

		switch (stateAction)
		{
		case StateAction_MarkWholeMirrorFullCopy:
			{
				MirroredObjectExistenceState newMirrorExistenceState;

				switch (state)
				{
				case PersistentFileSysState_Free:
					continue;

				case PersistentFileSysState_BulkLoadCreatePending:
				case PersistentFileSysState_CreatePending:
				case PersistentFileSysState_Created:
					// True in a way.  This will cause ReCreate.
					newMirrorExistenceState = MirroredObjectExistenceState_MirrorDownBeforeCreate;
					break;

				case PersistentFileSysState_DropPending:
				case PersistentFileSysState_AbortingCreate:
					if (mirrorExistenceState == MirroredObjectExistenceState_OnlyMirrorDropRemains)
					{
						PersistentFileSysState oldState;

						/*
						 * The primary enters this state when it has successfully dropped the primary object but
						 * was unable to drop the mirror object because the mirror was down.
						 *
						 * Since we have wipped out the mirror for the 'Full Copy' option, we can safely delete this persistent object.
						 */
						if (Debug_persistent_print)
							elog(Persistent_DebugPrintLevel(),
								 "StateAction_MarkWholeMirrorFullCopy: Removing '%s' as result of going to full copy, persistent state '%s', mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
								 PersistentFileSysObjName_ObjectName(&fsObjName),
								 PersistentFileSysObjState_Name(state),
								 MirroredObjectExistenceState_Name(mirrorExistenceState),
								 serialNum,
								 ItemPointerToString(&persistentTid));

						PersistentFileSysObj_StateChange(
													&fsObjName,
													&persistentTid,
													persistentSerialNum,
													PersistentFileSysState_Free,
													/* retryPossible */ false,
													/* flushToXLog */ false,
													&oldState,
													/* verifiedActionCallback */ NULL);
					}
					else
					{
						if (Debug_persistent_print)
							elog(Persistent_DebugPrintLevel(),
								 "StateAction_MarkWholeMirrorFullCopy: Skipping '%s' for full copy, persistent state '%s', mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
								 PersistentFileSysObjName_ObjectName(&fsObjName),
								 PersistentFileSysObjState_Name(state),
								 MirroredObjectExistenceState_Name(mirrorExistenceState),
								 serialNum,
								 ItemPointerToString(&persistentTid));

					}
					continue;

				default:
					elog(ERROR, "Unexpected persistent file-system object state: %d",
						 state);
                    newMirrorExistenceState = MaxMirroredObjectExistenceState;
				}

				if (fsObjType == PersistentFsObjType_RelationFile)
				{
					PersistentFileSysRelStorageMgr relStorageMgr;

					int64 moreBlocksToSynchronize;

					relStorageMgr =
						(PersistentFileSysRelStorageMgr)
										DatumGetInt16(
												values[Anum_gp_persistent_relation_node_relation_storage_manager - 1]);

					if (relStorageMgr == PersistentFileSysRelStorageMgr_BufferPool)
					{
						moreBlocksToSynchronize =
								PersistentFileSysObj_GetBufferPoolRelationTotalBlocks(
													PersistentFileSysObjName_GetRelFileNodePtr(&fsObjName));

					}
					else
					{
						int64 mirrorAppendOnlyNewEof;

						Assert(relStorageMgr == PersistentFileSysRelStorageMgr_AppendOnly);

						mirrorAppendOnlyNewEof = DatumGetInt64(values[Anum_gp_persistent_relation_node_mirror_append_only_new_eof - 1]);

						moreBlocksToSynchronize = (mirrorAppendOnlyNewEof + BLCKSZ - 1) / BLCKSZ;

					}

					FileRepResync_AddToTotalBlocksToSynchronize(moreBlocksToSynchronize);

					if (Debug_persistent_print)
						elog(Persistent_DebugPrintLevel(),
							 "StateAction_MarkWholeMirrorFullCopy: Mark '%s' full copy, persistent state '%s', relation storage manager '%s', number of 32k blocks " INT64_FORMAT ", old mirror existence state '%s', new mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
							 PersistentFileSysObjName_ObjectName(&fsObjName),
							 PersistentFileSysObjState_Name(state),
							 PersistentFileSysRelStorageMgr_Name(relStorageMgr),
							 moreBlocksToSynchronize,
							 MirroredObjectExistenceState_Name(mirrorExistenceState),
							 MirroredObjectExistenceState_Name(newMirrorExistenceState),
							 serialNum,
							 ItemPointerToString(&persistentTid));

				}
				else
				{
					if (Debug_persistent_print)
						elog(Persistent_DebugPrintLevel(),
							 "StateAction_MarkWholeMirrorFullCopy: Mark '%s' full copy, persistent state '%s', old mirror existence state '%s', new mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
							 PersistentFileSysObjName_ObjectName(&fsObjName),
							 PersistentFileSysObjState_Name(state),
							 MirroredObjectExistenceState_Name(mirrorExistenceState),
							 MirroredObjectExistenceState_Name(newMirrorExistenceState),
							 serialNum,
							 ItemPointerToString(&persistentTid));
				}

				tupleCopy = PersistentStore_GetScanTupleCopy(&storeScan);

				PersistentFileSysObj_UpdateTupleMirrorState(
														fileSysObjData,
														fileSysObjSharedData,
														fsObjType,
														tupleCopy,
														&persistentTid,
														persistentSerialNum,
														newMirrorExistenceState,
														MirroredRelDataSynchronizationState_FullCopy,
														/* flushToXLog */ false);

				heap_freetuple(tupleCopy);
			}
			break;

		case StateAction_MirrorReCreate:
			if (PersistentFileSysObj_IsMirrorReCreate(state, mirrorExistenceState))
			{
				MirrorDataLossTrackingState mirrorDataLossTrackingState;
				int64						mirrorDataLossTrackingSessionNum;

				bool mirrorDataLossOccurred;

				PersistentFileSysRelStorageMgr relStorageMgr = PersistentFileSysRelStorageMgr_None;

				if (fsObjType == PersistentFsObjType_RelationFile)
					relStorageMgr =
						(PersistentFileSysRelStorageMgr)
										DatumGetInt16(
												values[Anum_gp_persistent_relation_node_relation_storage_manager - 1]);

				if (Debug_persistent_print)
					elog(Persistent_DebugPrintLevel(),
						 "StateAction_MirrorReCreate: Attempt '%s' mirror re-create, persistent state '%s', mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
						 PersistentFileSysObjName_ObjectName(&fsObjName),
						 PersistentFileSysObjState_Name(state),
						 MirroredObjectExistenceState_Name(mirrorExistenceState),
						 serialNum,
						 ItemPointerToString(&persistentTid));

				// Don't hold lock during physical file-system operation.
				WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

				FileRepResync_SetReMirrorAllowed();

				// UNDONE: Pass this in from the resync worker?
				mirrorDataLossTrackingState =
							FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
															&mirrorDataLossTrackingSessionNum);

				PersistentFileSysObj_DoMirrorReCreate(
										&fsObjName,
										relStorageMgr,
										&persistentTid,
										persistentSerialNum,
										mirrorDataLossTrackingState,
										mirrorDataLossTrackingSessionNum,
										&mirrorDataLossOccurred);

				FileRepResync_ResetReMirrorAllowed();

				// UNDONE: Act on mirrorDataLossOccurred

				WRITE_PERSISTENT_STATE_ORDERED_LOCK;
				/*
				 * We calculate only RelationFile objects to create for resync.
				 * As it seems unnecessary overhead to scan database/tablespace/etc..
				 * Hence only decrement for RelationFile.
				 */
				if (fsObjType == PersistentFsObjType_RelationFile)
					FileRepResync_DecFsobjCount();
			}
			else
			{
				if (Debug_persistent_print)
					elog(Persistent_DebugPrintLevel(),
						 "StateAction_MirrorReCreate: Skip '%s' mirror re-create, persistent state '%s', mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
						 PersistentFileSysObjName_ObjectName(&fsObjName),
						 PersistentFileSysObjState_Name(state),
						 MirroredObjectExistenceState_Name(mirrorExistenceState),
						 serialNum,
						 ItemPointerToString(&persistentTid));

				if (filerep_mirrorvalidation_during_resync && (mirrorExistenceState == MirroredObjectExistenceState_MirrorCreated))
				{
					PersistentFileSysRelStorageMgr	relStorageMgr = PersistentFileSysRelStorageMgr_None;

					if (fsObjType == PersistentFsObjType_RelationFile)
						relStorageMgr =
									(PersistentFileSysRelStorageMgr)
									DatumGetInt16(
												  values[Anum_gp_persistent_relation_node_relation_storage_manager - 1]);

					FileRepResync_SetReMirrorAllowed();

					PersistentFileSysObj_DoMirrorValidation(
															&fsObjName,
															relStorageMgr,
															&persistentTid,
															persistentSerialNum);

					FileRepResync_ResetReMirrorAllowed();
				}
			}
			break;

		case StateAction_MarkMirrorReCreated:
			if ((state == PersistentFileSysState_BulkLoadCreatePending ||
				 state == PersistentFileSysState_CreatePending ||
				 state == PersistentFileSysState_Created ||
				 state == PersistentFileSysState_JustInTimeCreatePending)
				&&
				(mirrorExistenceState == MirroredObjectExistenceState_MirrorDownBeforeCreate ||
				 mirrorExistenceState == MirroredObjectExistenceState_MirrorDownDuringCreate))
			{
				PersistentFileSysRelStorageMgr relStorageMgr = PersistentFileSysRelStorageMgr_None;

				MirroredObjectExistenceState newMirrorExistenceState = MirroredObjectExistenceState_None;

				if (fsObjType == PersistentFsObjType_RelationFile)
					relStorageMgr =
						(PersistentFileSysRelStorageMgr)
										DatumGetInt16(
												values[Anum_gp_persistent_relation_node_relation_storage_manager - 1]);


				switch (state)
				{
				case PersistentFileSysState_BulkLoadCreatePending:
				case PersistentFileSysState_CreatePending:
				case PersistentFileSysState_JustInTimeCreatePending:
					newMirrorExistenceState = MirroredObjectExistenceState_MirrorCreatePending;
					break;

				case PersistentFileSysState_Created:
					newMirrorExistenceState = MirroredObjectExistenceState_MirrorCreated;
					break;

				default:
					newMirrorExistenceState = MirroredObjectExistenceState_None; /* make compiler happy */
					elog(ERROR, "Unexpected persistent state: %d",
						 state);
				}

				if (Debug_persistent_print)
					elog(Persistent_DebugPrintLevel(),
						 "StateAction_MarkMirrorReCreated: Mark '%s' mirror re-created, persistent state '%s', old mirror existence state '%s', new mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
						 PersistentFileSysObjName_ObjectName(&fsObjName),
						 PersistentFileSysObjState_Name(state),
						 MirroredObjectExistenceState_Name(mirrorExistenceState),
						 MirroredObjectExistenceState_Name(newMirrorExistenceState),
						 serialNum,
						 ItemPointerToString(&persistentTid));

				tupleCopy = PersistentStore_GetScanTupleCopy(&storeScan);

				PersistentFileSysObj_UpdateTupleMirrorReCreated(
														fileSysObjData,
														fileSysObjSharedData,
														fsObjType,
														tupleCopy,
														&persistentTid,
														persistentSerialNum,
														relStorageMgr,
														newMirrorExistenceState,
														/* flushToXLog */ false);

				heap_freetuple(tupleCopy);
			}
			else
			{
				if (Debug_persistent_print)
					elog(Persistent_DebugPrintLevel(),
						 "StateAction_MarkMirrorReCreated: Skip '%s' mirror re-create, persistent state '%s', mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
						 PersistentFileSysObjName_ObjectName(&fsObjName),
						 PersistentFileSysObjState_Name(state),
						 MirroredObjectExistenceState_Name(mirrorExistenceState),
						 serialNum,
						 ItemPointerToString(&persistentTid));
			}
			break;

		case StateAction_MirrorReDrop:
		case StateAction_MirrorReDropped:
			if (PersistentFileSysObj_IsMirrorReDrop(state, mirrorExistenceState))
			{
				MirrorDataLossTrackingState mirrorDataLossTrackingState;
				int64						mirrorDataLossTrackingSessionNum;

				PersistentFileSysRelStorageMgr relStorageMgr = PersistentFileSysRelStorageMgr_None;

				bool mirrorDataLossOccurred;

				if (fsObjType == PersistentFsObjType_RelationFile)
					relStorageMgr =
						(PersistentFileSysRelStorageMgr)
										DatumGetInt16(
												values[Anum_gp_persistent_relation_node_relation_storage_manager - 1]);

				if (Debug_persistent_print)
					elog(Persistent_DebugPrintLevel(),
						 "StateAction_MirrorReDrop: Attempt '%s' mirror re-drop, persistent state '%s', mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
						 PersistentFileSysObjName_ObjectName(&fsObjName),
						 PersistentFileSysObjState_Name(state),
						 MirroredObjectExistenceState_Name(mirrorExistenceState),
						 serialNum,
						 ItemPointerToString(&persistentTid));

				/*
				 * We'll not hold lock during physical file-system operation.
				 * Also note that we'll skip holding the lock even during Persistent
				 * Table update because DoMirroDropped() call internally acquires
				 * this lock.
				 */
				WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

				/*
				 * Based on if its 're-drop' phase or 're-dropped' phase either drop
				 * the mirror objects or simply update the Persistent Tables.
				 */
				if (StateAction_MirrorReDrop == stateAction)
				{
					/* Set this flag to allow actions on mirror during resync transition */
					FileRepResync_SetReMirrorAllowed();

					mirrorDataLossTrackingState =
								FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
																&mirrorDataLossTrackingSessionNum);

					// UNDONE: Pass this in from the resync worker?
					PersistentFileSysObj_DoMirrorReDrop(
											&fsObjName,
											relStorageMgr,
											&persistentTid,
											persistentSerialNum,
											mirrorDataLossTrackingState,
											mirrorDataLossTrackingSessionNum,
											&mirrorDataLossOccurred);

					// UNDONE: Act on mirrorDataLossOccurred
					FileRepResync_ResetReMirrorAllowed();

					/*
					 * We calculate only RelationFile objects to create for resync.
					 * As it seems unnecessary overhead to scan database/tablespace/etc..
					 * Hence only decrement for RelationFile.
					 */
					if (fsObjType == PersistentFsObjType_RelationFile)
					{
						FileRepResync_DecFsobjCount();
					}
				}
				else
				{
					PersistentFileSysObj_DoMirrorReDropped(
											&fsObjName,
											relStorageMgr,
											&persistentTid,
											persistentSerialNum);
				}

				/* Retain the lock */
				WRITE_PERSISTENT_STATE_ORDERED_LOCK;
			}
			else
			{
				if (Debug_persistent_print)
					elog(Persistent_DebugPrintLevel(),
						 "StateAction_MirrorReDrop: Skip '%s' mirror re-drop, persistent state '%s', mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
						 PersistentFileSysObjName_ObjectName(&fsObjName),
						 PersistentFileSysObjState_Name(state),
						 MirroredObjectExistenceState_Name(mirrorExistenceState),
						 serialNum,
						 ItemPointerToString(&persistentTid));
			}
			break;

		case StateAction_MirrorAdd:
			if (state == PersistentFileSysState_Free)
				continue;

			/*
			 * Change mirror existence state from 'Not Configured' to 'Mirror Down Before Create'
			 * to indicate nothing exists on the mirror yet.
			 */

			tupleCopy = PersistentStore_GetScanTupleCopy(&storeScan);

			PersistentFileSysObj_UpdateTupleMirrorExistenceState(
													fileSysObjData,
													fileSysObjSharedData,
													fsObjType,
													tupleCopy,
													&persistentTid,
													persistentSerialNum,
													MirroredObjectExistenceState_MirrorDownBeforeCreate,
													/* flushToXLog */ false);

			heap_freetuple(tupleCopy);
			break;

		default:
			elog(ERROR, "Unexpected state-action: %d",
				 stateAction);
		}

		WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

		WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	}

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	PersistentStore_EndScan(&storeScan);

	pfree(values);
}

void PersistentFileSysObj_MarkWholeMirrorFullCopy(void)
{
	PersistentFsObjType		fsObjType;

	PersistentFileSysObj_VerifyInitScan();

	for (fsObjType = PersistentFsObjType_First;
		 fsObjType <= PersistentFsObjType_Last;
		 fsObjType++)
 	{
 		PersistentFileSysObj_ScanStateAction(
										StateAction_MarkWholeMirrorFullCopy,
										fsObjType);
 	}
}

void PersistentFileSysObj_MirrorReCreate(void)
{
	PersistentFsObjType		fsObjType;

	/*
	 * For CREATE, we go top-down.
	 */
	fsObjType = PersistentFsObjType_Last;
	while (true)
	{
		/*
		 * Since PersistentFsObjType is an enumeration type and treated by the
		 * compiler as unsigned, we must be careful to not decrement below 0...
		 * Or, it will wrap-around to 4 billion or negative 2 billion.  Didn't PASCAL
		 * have cool iterators?  Perhaps not.
		 */
		PersistentFileSysObj_ScanStateAction(
										StateAction_MirrorReCreate,
										fsObjType);
		if (fsObjType == PersistentFsObjType_First)
			break;
		fsObjType--;
	}
}

void PersistentFileSysObj_MarkMirrorReCreated(void)
{
	PersistentFsObjType		fsObjType;

	/*
	 * For CREATE, we go top-down.
	 */
	fsObjType = PersistentFsObjType_Last;
	while (true)
	{
		/*
		 * Since PersistentFsObjType is an enumeration type and treated by the
		 * compiler as unsigned, we must be careful to not decrement below 0...
		 * Or, it will wrap-around to 4 billion or negative 2 billion.  Didn't PASCAL
		 * have cool iterators?  Perhaps not.
		 */
		PersistentFileSysObj_ScanStateAction(
										StateAction_MarkMirrorReCreated,
										fsObjType);
		if (fsObjType == PersistentFsObjType_First)
			break;
		fsObjType--;
	}
}

/*
 * Count the number of file system objects to be created/dropped on
 * mirror prior to resync phase.
 */
int PersistentFileSysObj_CountResyncObjects(void)
{
	PersistentFileSysObjData *fileSysObjData;
	PersistentFileSysObjSharedData	*fileSysObjSharedData;

	PersistentStoreScan storeScan;
	Datum values[Natts_gp_persistent_relation_node];

	ItemPointerData persistentTid;
	int64 persistentSerialNum;
	MirroredObjectExistenceState		mirrorExistenceState;
	PersistentFileSysState  			state;
	PersistentFileSysObjName			fsObjName;
	TransactionId						parentXid;
	int64								serialNum;
	int									count = 0;

	PersistentFileSysObj_GetDataPtrs(
								PersistentFsObjType_RelationFile,
								&fileSysObjData,
								&fileSysObjSharedData);

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
		GpPersistent_GetCommonValues(
								PersistentFsObjType_RelationFile,
								values,
								&fsObjName,
								&state,
								&mirrorExistenceState,
								&parentXid,
								&serialNum);
		if (PersistentFileSysObj_IsMirrorReDrop(state, mirrorExistenceState) ||
			PersistentFileSysObj_IsMirrorReCreate(state, mirrorExistenceState))
		{
			count++;
		}
	}
	PersistentStore_EndScan(&storeScan);
	return count;
}

void PersistentFileSysObj_MirrorReDrop(void)
{
	PersistentFsObjType 	fsObjType;

	/*
	 * For DROP, we go bottom-up.
	 */
	for (fsObjType = PersistentFsObjType_First;
		 fsObjType <= PersistentFsObjType_Last;
		 fsObjType++)
	{
		PersistentFileSysObj_ScanStateAction(
										StateAction_MirrorReDrop,
										fsObjType);
	}
}

void PersistentFileSysObj_MarkMirrorReDropped(void)
{
	PersistentFsObjType 	fsObjType;

	/*
	 * For DROPPED, we go bottom-up.
	 */
	for (fsObjType = PersistentFsObjType_First;
		 fsObjType <= PersistentFsObjType_Last;
		 fsObjType++)
	{
		PersistentFileSysObj_ScanStateAction(
										StateAction_MirrorReDropped,
										fsObjType);
	}
}


/*
 * Mark the special persistent tables as needing 'Scan Incremental' recovery.
 */
void PersistentFileSysObj_MarkSpecialScanIncremental(void)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjData *fileSysObjData;
	PersistentFileSysObjSharedData	*fileSysObjSharedData;

	PersistentStoreScan storeScan;

	HeapTuple tupleCopy;

	Datum values[Natts_gp_persistent_relation_node];

	ItemPointerData persistentTid;
	int64 persistentSerialNum;

	tupleCopy = NULL;

	PersistentFileSysObj_VerifyInitScan();

	PersistentFileSysObj_GetDataPtrs(
								PersistentFsObjType_RelationFile,
								&fileSysObjData,
								&fileSysObjSharedData);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

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
		RelFileNode 					relFileNode;
		int32 							segmentFileNum;

		PersistentFileSysRelStorageMgr	relationStorageManager;
		PersistentFileSysState			persistentState;
		int64							createMirrorDataLossTrackingSessionNum;
		MirroredObjectExistenceState	mirrorExistenceState;
		MirroredRelDataSynchronizationState mirrorDataSynchronizationState;
		bool							mirrorBufpoolMarkedForScanIncrementalResync;
		int64							mirrorBufpoolResyncChangedPageCount;
		XLogRecPtr						mirrorBufpoolResyncCkptLoc;
		BlockNumber 					mirrorBufpoolResyncCkptBlockNum;
		int64							mirrorAppendOnlyLossEof;
		int64							mirrorAppendOnlyNewEof;
		PersistentFileSysRelBufpoolKind relBufpoolKind;
		TransactionId					parentXid;
		int64							serialNum;
		PersistentFileSysObjName		fsObjName;

		int32							numOf32kBlocks;

		GpPersistentRelationNode_GetValues(
										values,
										&relFileNode.spcNode,
										&relFileNode.dbNode,
										&relFileNode.relNode,
										&segmentFileNum,
										&relationStorageManager,
										&persistentState,
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

		PersistentFileSysObjName_SetRelationFile(
											&fsObjName,
											&relFileNode,
											segmentFileNum);

		/*
		 * We need 'Scan Incremental' for the special relations that we don't cover with the
		 * change tracking log.
		 */
		if (GpPersistent_SkipXLogInfo(relFileNode.relNode))
		{

			tupleCopy = PersistentStore_GetScanTupleCopy(&storeScan);

			PersistentFileSysObj_UpdateTupleRelDataSynchronizationState(
													fileSysObjData,
													fileSysObjSharedData,
													tupleCopy,
													&persistentTid,
													persistentSerialNum,
													MirroredRelDataSynchronizationState_BufferPoolScanIncremental,
													/* flushToXLog */ false);

			heap_freetuple(tupleCopy);

			numOf32kBlocks =
					PersistentFileSysObj_GetBufferPoolRelationTotalBlocks(
										PersistentFileSysObjName_GetRelFileNodePtr(&fsObjName));

			FileRepResync_AddToTotalBlocksToSynchronize(numOf32kBlocks);

			if (Debug_persistent_print)
				elog(Persistent_DebugPrintLevel(),
					 "PersistentFileSysObj_MarkSpecialScanIncremental: Mark '%s' needing special scan incremental, number of 32k blocks %d, persistent state '%s', mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
					 PersistentFileSysObjName_ObjectName(&fsObjName),
					 numOf32kBlocks,
					 PersistentFileSysObjState_Name(persistentState),
					 MirroredObjectExistenceState_Name(mirrorExistenceState),
					 serialNum,
					 ItemPointerToString(&persistentTid));

		}
		else if (persistentState != PersistentFileSysState_BulkLoadCreatePending &&
				 MirroredObjectExistenceState_IsResynchCreated(mirrorExistenceState) &&
				 mirrorBufpoolMarkedForScanIncrementalResync)
		{
			/*
			 * If this is a relation that ~_ResynchronizeScan would choose and it is marked
			 * needs be resynchronized with 'Scan Incremental', then mark and examine
			 * the file EOFs.
			 */

			tupleCopy = PersistentStore_GetScanTupleCopy(&storeScan);

			PersistentFileSysObj_UpdateTupleScanIncrementalAndResetFlag(
													fileSysObjData,
													fileSysObjSharedData,
													tupleCopy,
													&persistentTid,
													persistentSerialNum,
													/* flushToXLog */ false);

			heap_freetuple(tupleCopy);

			numOf32kBlocks =
					PersistentFileSysObj_GetBufferPoolRelationTotalBlocks(
										PersistentFileSysObjName_GetRelFileNodePtr(&fsObjName));

			FileRepResync_AddToTotalBlocksToSynchronize(numOf32kBlocks);

			if (Debug_persistent_print)
				elog(Persistent_DebugPrintLevel(),
					 "PersistentFileSysObj_MarkSpecialScanIncremental: Mark '%s' vacuumed relation as scan incremental, number of 32k blocks %d, persistent state '%s', mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
					 PersistentFileSysObjName_ObjectName(&fsObjName),
					 numOf32kBlocks,
					 PersistentFileSysObjState_Name(persistentState),
					 MirroredObjectExistenceState_Name(mirrorExistenceState),
					 serialNum,
					 ItemPointerToString(&persistentTid));

		}

		WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

		WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	}

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	PersistentStore_EndScan(&storeScan);
}

/*
 * Mark any Append-Only tables that have mirror data loss as needing 'Append-Only Catch-Up' recovery.
 */
void PersistentFileSysObj_MarkAppendOnlyCatchup(void)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjData *fileSysObjData;
	PersistentFileSysObjSharedData	*fileSysObjSharedData;

	PersistentStoreScan storeScan;

	HeapTuple tupleCopy;

	Datum values[Natts_gp_persistent_relation_node];

	ItemPointerData persistentTid;
	int64 persistentSerialNum;

	tupleCopy = NULL;

	PersistentFileSysObj_VerifyInitScan();

	PersistentFileSysObj_GetDataPtrs(
								PersistentFsObjType_RelationFile,
								&fileSysObjData,
								&fileSysObjSharedData);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

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
		RelFileNode 					relFileNode;
		int32 							segmentFileNum;

		PersistentFileSysRelStorageMgr	relationStorageManager;
		PersistentFileSysState			persistentState;
		int64							createMirrorDataLossTrackingSessionNum;
		MirroredObjectExistenceState	mirrorExistenceState;
		MirroredRelDataSynchronizationState mirrorDataSynchronizationState;
		bool							mirrorBufpoolMarkedForScanIncrementalResync;
		int64							mirrorBufpoolResyncChangedPageCount;
		XLogRecPtr						mirrorBufpoolResyncCkptLoc;
		BlockNumber 					mirrorBufpoolResyncCkptBlockNum;
		int64							mirrorAppendOnlyLossEof;
		int64							mirrorAppendOnlyNewEof;
		PersistentFileSysRelBufpoolKind relBufpoolKind;
		TransactionId					parentXid;
		int64							serialNum;
		PersistentFileSysObjName		fsObjName;

		GpPersistentRelationNode_GetValues(
										values,
										&relFileNode.spcNode,
										&relFileNode.dbNode,
										&relFileNode.relNode,
										&segmentFileNum,
										&relationStorageManager,
										&persistentState,
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

		if (persistentState == PersistentFileSysState_Free)
			continue;

		PersistentFileSysObjName_SetRelationFile(
											&fsObjName,
											&relFileNode,
											segmentFileNum);

		if (relationStorageManager == PersistentFileSysRelStorageMgr_AppendOnly
			&&
			persistentState == PersistentFileSysState_Created
			&&
			MirroredObjectExistenceState_IsResynchCreated(mirrorExistenceState))
		{
			/*
			 * Note that 'Create Pending' Append-Only relation will use the Append-Only
			 * commit count and notify resynchronize of more files to resynchronize on
			 * commit.  Or, they will copy over new data to the mirror themselves.
			 */
			if (mirrorAppendOnlyLossEof < mirrorAppendOnlyNewEof)
			{
				int64 roundedUp32kBlocks;

				tupleCopy = PersistentStore_GetScanTupleCopy(&storeScan);

				PersistentFileSysObj_UpdateTupleRelDataSynchronizationState(
														fileSysObjData,
														fileSysObjSharedData,
														tupleCopy,
														&persistentTid,
														persistentSerialNum,
														MirroredRelDataSynchronizationState_AppendOnlyCatchup,
														/* flushToXLog */ false);

				heap_freetuple(tupleCopy);

				roundedUp32kBlocks = (mirrorAppendOnlyNewEof - mirrorAppendOnlyLossEof + BLCKSZ - 1) / BLCKSZ;

				FileRepResync_AddToTotalBlocksToSynchronize(roundedUp32kBlocks);

				if (Debug_persistent_print)
					elog(Persistent_DebugPrintLevel(),
						 "PersistentFileSysObj_MarkAppendOnlyCatchup: Mark '%s' Append-Only relation as needing catch-up (mirror loss EOF " INT64_FORMAT ", mirror new EOF " INT64_FORMAT ", number of 32k blocks " INT64_FORMAT "), persistent state '%s', mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
						 PersistentFileSysObjName_ObjectName(&fsObjName),
						 mirrorAppendOnlyLossEof,
						 mirrorAppendOnlyNewEof,
						 roundedUp32kBlocks,
						 PersistentFileSysObjState_Name(persistentState),
						 MirroredObjectExistenceState_Name(mirrorExistenceState),
						 serialNum,
						 ItemPointerToString(&persistentTid));

			}
			else
			{
				if (Debug_persistent_print)
					elog(Persistent_DebugPrintLevel(),
						 "PersistentFileSysObj_MarkAppendOnlyCatchup: Found '%s' Append-Only relation not needing catch-up (mirror loss EOF " INT64_FORMAT ", mirror new EOF " INT64_FORMAT "), persistent state '%s', mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
						 PersistentFileSysObjName_ObjectName(&fsObjName),
						 mirrorAppendOnlyLossEof,
						 mirrorAppendOnlyNewEof,
						 PersistentFileSysObjState_Name(persistentState),
						 MirroredObjectExistenceState_Name(mirrorExistenceState),
						 serialNum,
						 ItemPointerToString(&persistentTid));
			}

		}
		else
		{
			if (Debug_persistent_print)
				elog(Persistent_DebugPrintLevel(),
					 "PersistentFileSysObj_MarkAppendOnlyCatchup: Skipping '%s' for Append-Only, relation storage manager '%s', persistent state '%s', mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
					 PersistentFileSysObjName_ObjectName(&fsObjName),
					 PersistentFileSysRelStorageMgr_Name(relationStorageManager),
					 PersistentFileSysObjState_Name(persistentState),
					 MirroredObjectExistenceState_Name(mirrorExistenceState),
					 serialNum,
					 ItemPointerToString(&persistentTid));
		}

		WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

		WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	}

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	PersistentStore_EndScan(&storeScan);
}


static void PersistentFileSysObj_MarkPageIncremental(
	ItemPointer			persistentTid,
	int64				persistentSerialNum,
	int64				newMirrorBufpoolResyncChangedPageCount)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	READTUPLE_FOR_UPDATE_ERRCONTEXT_DECLARE;

	PersistentFileSysObjData 		*fileSysObjData;
	PersistentFileSysObjSharedData 	*fileSysObjSharedData;

	Datum values[Natts_gp_persistent_relation_node];

	HeapTuple tupleCopy;

	RelFileNode 					relFileNode;
	int32							segmentFileNum;

	PersistentFileSysRelStorageMgr	relationStorageManager;
	PersistentFileSysState			persistentState;
	int64							createMirrorDataLossTrackingSessionNum;
	MirroredObjectExistenceState	mirrorExistenceState;
	MirroredRelDataSynchronizationState mirrorDataSynchronizationState;
	bool							mirrorBufpoolMarkedForScanIncrementalResync;
	int64							currentMirrorBufpoolResyncChangedPageCount;
	XLogRecPtr						mirrorBufpoolResyncCkptLoc;
	BlockNumber 					mirrorBufpoolResyncCkptBlockNum;
	int64							mirrorAppendOnlyLossEof;
	int64							mirrorAppendOnlyNewEof;
	PersistentFileSysRelBufpoolKind relBufpoolKind;
	TransactionId					parentXid;
	int64							serialNum;
	PersistentFileSysObjName		fsObjName;

	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent '%s' tuple for marking page incremental is invalid (0,0)",
			 PersistentFileSysObjName_TypeName(PersistentFsObjType_RelationFile));

	if (persistentSerialNum == 0)
		elog(ERROR, "Persistent '%s' serial number for update mirror existence state is invalid (0)",
			 PersistentFileSysObjName_TypeName(PersistentFsObjType_RelationFile));

	PersistentFileSysObj_GetDataPtrs(
								PersistentFsObjType_RelationFile,
								&fileSysObjData,
								&fileSysObjSharedData);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	READTUPLE_FOR_UPDATE_ERRCONTEXT_PUSHTYPE(PersistentFsObjType_RelationFile, persistentTid, persistentSerialNum);

	PersistentFileSysObj_ReadTuple(
							PersistentFsObjType_RelationFile,
							persistentTid,
							values,
							&tupleCopy);

	READTUPLE_FOR_UPDATE_ERRCONTEXT_POP;

	if (tupleCopy == NULL)
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "PersistentFileSysObj_MarkPageIncremental: Skipping 'Free' entry at TID %s",
				 ItemPointerToString(persistentTid));

		WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;
		return;
	}

	GpPersistentRelationNode_GetValues(
									values,
									&relFileNode.spcNode,
									&relFileNode.dbNode,
									&relFileNode.relNode,
									&segmentFileNum,
									&relationStorageManager,
									&persistentState,
									&createMirrorDataLossTrackingSessionNum,
									&mirrorExistenceState,
									&mirrorDataSynchronizationState,
									&mirrorBufpoolMarkedForScanIncrementalResync,
									&currentMirrorBufpoolResyncChangedPageCount,
									&mirrorBufpoolResyncCkptLoc,
									&mirrorBufpoolResyncCkptBlockNum,
									&mirrorAppendOnlyLossEof,
									&mirrorAppendOnlyNewEof,
									&relBufpoolKind,
									&parentXid,
									&serialNum);

	if (serialNum != persistentSerialNum)
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "PersistentFileSysObj_MarkPageIncremental: Skipping obsolete serial number (input serial number " INT64_FORMAT ", actual serial number " INT64_FORMAT ") at TID %s",
				 persistentSerialNum,
				 serialNum,
				 ItemPointerToString(persistentTid));

		heap_freetuple(tupleCopy);

		WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;
		return;
	}

	PersistentFileSysObjName_SetRelationFile(
										&fsObjName,
										&relFileNode,
										segmentFileNum);

	if (relationStorageManager == PersistentFileSysRelStorageMgr_BufferPool
		&&
		(persistentState == PersistentFileSysState_CreatePending ||
		 persistentState == PersistentFileSysState_Created)
		&&
		MirroredObjectExistenceState_IsResynchCreated(mirrorExistenceState)
		&&
		mirrorDataSynchronizationState != MirroredRelDataSynchronizationState_FullCopy
		&&
		mirrorDataSynchronizationState != MirroredRelDataSynchronizationState_BufferPoolScanIncremental)
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "PersistentFileSysObj_MarkPageIncremental: Mark '%s' needing page incremental (changed page count " INT64_FORMAT "), "
				 "persistent state '%s', mirror existence state '%s', mirror synchronization state '%s', serial number " INT64_FORMAT " at TID %s",
				 PersistentFileSysObjName_ObjectName(&fsObjName),
				 newMirrorBufpoolResyncChangedPageCount,
				 PersistentFileSysObjState_Name(persistentState),
				 MirroredObjectExistenceState_Name(mirrorExistenceState),
				 MirroredRelDataSynchronizationState_Name(mirrorDataSynchronizationState),
				 serialNum,
				 ItemPointerToString(persistentTid));

		PersistentFileSysObj_UpdateTuplePageIncremental(
												fileSysObjData,
												fileSysObjSharedData,
												tupleCopy,
												persistentTid,
												persistentSerialNum,
												newMirrorBufpoolResyncChangedPageCount,
												/* flushToXLog */ false);
	}
	else
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "PersistentFileSysObj_MarkPageIncremental: Skipping '%s' for page incremental, relation storage manager '%s', "
				 "persistent state '%s', mirror existence state '%s', mirror synchronization state '%s', serial number " INT64_FORMAT " at TID %s",
				 PersistentFileSysObjName_ObjectName(&fsObjName),
				 PersistentFileSysRelStorageMgr_Name(relationStorageManager),
				 PersistentFileSysObjState_Name(persistentState),
				 MirroredObjectExistenceState_Name(mirrorExistenceState),
				 MirroredRelDataSynchronizationState_Name(mirrorDataSynchronizationState),
				 serialNum,
				 ItemPointerToString(persistentTid));

		heap_freetuple(tupleCopy);
	}

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

}

void PersistentFileSysObj_MarkPageIncrementalFromChangeLog(void)
{
	IncrementalChangeList*	incrementalChangeList;
	int 					count = 0;
	int 					i;

	PersistentFileSysObj_VerifyInitScan();

	/*
	 * Finalize the compacting steps of the changetracking logs. after
	 * this call we'll have the smallest possible log file to work with.
	 * we pass in a NULL lsn to signify we want all CT records.
	 */
	ChangeTracking_DoFullCompactingRound(NULL);
	if (segmentState == SegmentStateChangeTrackingDisabled)
	{
		elog(LOG,
			 "PersistentFileSysObj_MarkPageIncrementalFromChangeLog returning as SegmentState is ChangeTrackingDisabled");
		return;
	}

	/* get a list of relations that had 1 or more blocks changed */
	incrementalChangeList = ChangeTracking_GetIncrementalChangeList();

	if (incrementalChangeList != NULL)
	{
		count = incrementalChangeList->count;
	}

	elog(LOG, "ChangeTracking_GetIncrementalChangeList returned %d entries", count);

	for (i = 0; i < count; i++)
	{
		if (PersistentStore_IsZeroTid(&incrementalChangeList->entries[i].persistentTid))
		{
			elog(WARNING, "TID for persistent '%s' tuple from change log is invalid (0,0)",
				 PersistentFileSysObjName_TypeName(PersistentFsObjType_RelationFile));
			continue;
		}

		if (incrementalChangeList->entries[i].persistentSerialNum == 0)
		{
			elog(WARNING, "Persistent '%s' serial number from change log is invalid (0)",
				 PersistentFileSysObjName_TypeName(PersistentFsObjType_RelationFile));
			continue;
		}

		Assert(incrementalChangeList->entries[i].numblocks > 0);

		if (Debug_filerep_config_print)
		{
			elog(LOG,
				 "resync TID %s serial number " INT64_FORMAT " numBlocks changed " INT64_FORMAT "",
				 ItemPointerToString(&incrementalChangeList->entries[i].persistentTid),
				 incrementalChangeList->entries[i].persistentSerialNum,
				 incrementalChangeList->entries[i].numblocks);
		}

		PersistentFileSysObj_MarkPageIncremental(
											&incrementalChangeList->entries[i].persistentTid,
											incrementalChangeList->entries[i].persistentSerialNum,
											incrementalChangeList->entries[i].numblocks);

		FileRepResync_AddToTotalBlocksToSynchronize(incrementalChangeList->entries[i].numblocks);
	}

	if (incrementalChangeList != NULL)
	{
		ChangeTracking_FreeIncrementalChangeList(incrementalChangeList);
	}
}

bool PersistentFileSysObj_ResynchronizeScan(
	ResynchronizeScanToken			*resynchronizeScanToken,
	RelFileNode						*relFileNode,
	int32							*segmentFileNum,
	PersistentFileSysRelStorageMgr 	*relStorageMgr,
	MirroredRelDataSynchronizationState *mirrorDataSynchronizationState,
	int64							*mirrorBufpoolResyncChangedPageCount,
	XLogRecPtr						*mirrorBufpoolResyncCkptLoc,
	BlockNumber 					*mirrorBufpoolResyncCkptBlockNum,
	int64							*mirrorAppendOnlyLossEof,
	int64							*mirrorAppendOnlyNewEof,
	ItemPointer						persistentTid,
	int64							*persistentSerialNum)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjData *fileSysObjData;
	PersistentFileSysObjSharedData	*fileSysObjSharedData;

	Datum values[Natts_gp_persistent_relation_node];

	PersistentFileSysState			state;
	int64							createMirrorDataLossTrackingSessionNum;
	MirroredObjectExistenceState	mirrorExistenceState;
	bool							mirrorBufpoolMarkedForScanIncrementalResync;
	TransactionId					parentXid;
	int64							serialNum;
	PersistentFileSysRelBufpoolKind relBufpoolKind;

	PersistentFileSysObjName		fsObjName;

	if (resynchronizeScanToken->done)
		return false;

	PersistentFileSysObj_GetDataPtrs(
								PersistentFsObjType_RelationFile,
								&fileSysObjData,
								&fileSysObjSharedData);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	if (!resynchronizeScanToken->beginScan)
	{
		PersistentStore_BeginScan(
							&fileSysObjData->storeData,
							&fileSysObjSharedData->storeSharedData,
							&resynchronizeScanToken->storeScan);

		resynchronizeScanToken->beginScan = true;
	}

	while (true)
	{
		if (!PersistentStore_GetNext(
								&resynchronizeScanToken->storeScan,
								values,
								persistentTid,
								persistentSerialNum))
		{
			PersistentStore_EndScan(&resynchronizeScanToken->storeScan);
			resynchronizeScanToken->beginScan = false;
			resynchronizeScanToken->done = true;

			WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

			return false;
		}

		GpPersistentRelationNode_GetValues(
										values,
										&relFileNode->spcNode,
										&relFileNode->dbNode,
										&relFileNode->relNode,
										segmentFileNum,
										relStorageMgr,
										&state,
										&createMirrorDataLossTrackingSessionNum,
										&mirrorExistenceState,
										mirrorDataSynchronizationState,
										&mirrorBufpoolMarkedForScanIncrementalResync,
										mirrorBufpoolResyncChangedPageCount,
										mirrorBufpoolResyncCkptLoc,
										mirrorBufpoolResyncCkptBlockNum,
										mirrorAppendOnlyLossEof,
										mirrorAppendOnlyNewEof,
										&relBufpoolKind,
										&parentXid,
										&serialNum);

		if (state == PersistentFileSysState_Free)
			continue;

		PersistentFileSysObjName_SetRelationFile(
											&fsObjName,
											relFileNode,
											*segmentFileNum);

		if (state != PersistentFileSysState_BulkLoadCreatePending &&
			MirroredObjectExistenceState_IsResynchCreated(mirrorExistenceState))
		{

			if (Debug_persistent_print)
				elog(Persistent_DebugPrintLevel(),
					 "PersistentFileSysObj_ResynchronizeScan: Return '%s' needing data resynchronization ('%s'), persistent state '%s', mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
					 PersistentFileSysObjName_ObjectName(&fsObjName),
					 MirroredRelDataSynchronizationState_Name(*mirrorDataSynchronizationState),
					 PersistentFileSysObjState_Name(state),
					 MirroredObjectExistenceState_Name(mirrorExistenceState),
					 serialNum,
					 ItemPointerToString(persistentTid));

			break;
		}
		else
		{
			if (Debug_persistent_print)
				elog(Persistent_DebugPrintLevel(),
					 "PersistentFileSysObj_ResynchronizeScan: Skip '%s' -- doesn't need data resynchronization ('%s'), persistent state '%s', mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
					 PersistentFileSysObjName_ObjectName(&fsObjName),
					 MirroredRelDataSynchronizationState_Name(*mirrorDataSynchronizationState),
					 PersistentFileSysObjState_Name(state),
					 MirroredObjectExistenceState_Name(mirrorExistenceState),
					 serialNum,
					 ItemPointerToString(persistentTid));
		}

		WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

		WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	}

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	return true;
}

/*
 * Refetch the resynchronize relation information while under RELATION_RESYNCHRONIZE lock
 * based on its persistent TID and serial number.
 */
bool PersistentFileSysObj_ResynchronizeRefetch(
	RelFileNode						*relFileNode,
	int32							*segmentFileNum,
	ItemPointer						persistentTid,
	int64							persistentSerialNum,
	PersistentFileSysRelStorageMgr 	*relStorageMgr,
	MirroredRelDataSynchronizationState *mirrorDataSynchronizationState,
	XLogRecPtr						*mirrorBufpoolResyncCkptLoc,
	BlockNumber 					*mirrorBufpoolResyncCkptBlockNum,
	int64							*mirrorAppendOnlyLossEof,
	int64							*mirrorAppendOnlyNewEof)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	READTUPLE_FOR_UPDATE_ERRCONTEXT_DECLARE;

	PersistentFileSysObjData 		*fileSysObjData;
	PersistentFileSysObjSharedData 	*fileSysObjSharedData;

	Datum values[Natts_gp_persistent_relation_node];

	HeapTuple tupleCopy;

	RelFileNode 					actualRelFileNode;
	int32							actualSegmentFileNum;

	PersistentFileSysState			persistentState;
	int64							createMirrorDataLossTrackingSessionNum;
	MirroredObjectExistenceState	mirrorExistenceState;
	bool							mirrorBufpoolMarkedForScanIncrementalResync;
	int64							mirrorBufpoolResyncChangedPageCount;
	PersistentFileSysRelBufpoolKind relBufpoolKind;
	TransactionId					parentXid;
	int64							serialNum;

	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent '%s' tuple for marking page incremental is invalid (0,0)",
			 PersistentFileSysObjName_TypeName(PersistentFsObjType_RelationFile));

	if (persistentSerialNum == 0)
		elog(ERROR, "Persistent '%s' serial number for update mirror existence state is invalid (0)",
			 PersistentFileSysObjName_TypeName(PersistentFsObjType_RelationFile));

	PersistentFileSysObj_VerifyInitScan();

	PersistentFileSysObj_GetDataPtrs(
								PersistentFsObjType_RelationFile,
								&fileSysObjData,
								&fileSysObjSharedData);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	READTUPLE_FOR_UPDATE_ERRCONTEXT_PUSHTYPE(PersistentFsObjType_RelationFile, persistentTid, persistentSerialNum);

	PersistentFileSysObj_ReadTuple(
							PersistentFsObjType_RelationFile,
							persistentTid,
							values,
							&tupleCopy);

	READTUPLE_FOR_UPDATE_ERRCONTEXT_POP;

	if (tupleCopy == NULL)
		return false;

	GpPersistentRelationNode_GetValues(
									values,
									&actualRelFileNode.spcNode,
									&actualRelFileNode.dbNode,
									&actualRelFileNode.relNode,
									&actualSegmentFileNum,
									relStorageMgr,
									&persistentState,
									&createMirrorDataLossTrackingSessionNum,
									&mirrorExistenceState,
									mirrorDataSynchronizationState,
									&mirrorBufpoolMarkedForScanIncrementalResync,
									&mirrorBufpoolResyncChangedPageCount,
									mirrorBufpoolResyncCkptLoc,
									mirrorBufpoolResyncCkptBlockNum,
									mirrorAppendOnlyLossEof,
									mirrorAppendOnlyNewEof,
									&relBufpoolKind,
									&parentXid,
									&serialNum);

	heap_freetuple(tupleCopy);

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (persistentSerialNum != serialNum)
		return false;

	return true;
}

void PersistentFileSysObj_ResynchronizeRelationComplete(
	ItemPointer 					persistentTid,
	int64							persistentSerialNum,
	int64							mirrorLossEof,
	bool							flushToXLog)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	READTUPLE_FOR_UPDATE_ERRCONTEXT_DECLARE;

	PersistentFileSysObjData 		*fileSysObjData;
	PersistentFileSysObjSharedData 	*fileSysObjSharedData;

	Datum values[Natts_gp_persistent_relation_node];

	HeapTuple tupleCopy;

	Datum newValues[Natts_gp_persistent_relation_node];
	bool replaces[Natts_gp_persistent_relation_node];

	XLogRecPtr mirrorBufpoolResyncCkptLoc;

	PersistentFileSysRelStorageMgr relStorageMgr;

	if (PersistentStore_IsZeroTid(persistentTid))
		elog(ERROR, "TID for persistent '%s' tuple for resynchronize relation complete is invalid (0,0)",
			 PersistentFileSysObjName_TypeName(PersistentFsObjType_RelationFile));

	if (persistentSerialNum == 0)
		elog(ERROR, "Persistent '%s' serial number for resynchronize relation complete is invalid (0)",
			 PersistentFileSysObjName_TypeName(PersistentFsObjType_RelationFile));

	MemSet(&mirrorBufpoolResyncCkptLoc, 0, sizeof(mirrorBufpoolResyncCkptLoc));

	PersistentFileSysObj_GetDataPtrs(
								PersistentFsObjType_RelationFile,
								&fileSysObjData,
								&fileSysObjSharedData);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	READTUPLE_FOR_UPDATE_ERRCONTEXT_PUSHTYPE(PersistentFsObjType_RelationFile, persistentTid, persistentSerialNum);

	PersistentFileSysObj_ReadTuple(
							PersistentFsObjType_RelationFile,
							persistentTid,
							values,
							&tupleCopy);

	READTUPLE_FOR_UPDATE_ERRCONTEXT_POP;

	MemSet(newValues, 0, Natts_gp_persistent_relation_node * sizeof(Datum));
	MemSet(replaces, 0, Natts_gp_persistent_relation_node * sizeof(bool));

	replaces[Anum_gp_persistent_relation_node_mirror_data_synchronization_state - 1] = true;
	newValues[Anum_gp_persistent_relation_node_mirror_data_synchronization_state - 1] =
									Int16GetDatum(MirroredRelDataSynchronizationState_DataSynchronized);

	relStorageMgr =
		(PersistentFileSysRelStorageMgr)
						DatumGetInt16(
								values[Anum_gp_persistent_relation_node_relation_storage_manager - 1]);

	if (relStorageMgr == PersistentFileSysRelStorageMgr_BufferPool)
	{

		replaces[Anum_gp_persistent_relation_node_mirror_bufpool_resync_changed_page_count - 1] = true;
		newValues[Anum_gp_persistent_relation_node_mirror_bufpool_resync_changed_page_count - 1] =
										Int64GetDatum(0);

		replaces[Anum_gp_persistent_relation_node_mirror_bufpool_resync_ckpt_loc - 1] = true;
		newValues[Anum_gp_persistent_relation_node_mirror_bufpool_resync_ckpt_loc - 1] =
										PointerGetDatum(&mirrorBufpoolResyncCkptLoc);

		replaces[Anum_gp_persistent_relation_node_mirror_bufpool_resync_ckpt_block_num - 1] = true;
		newValues[Anum_gp_persistent_relation_node_mirror_bufpool_resync_ckpt_block_num - 1] =
										Int32GetDatum(0);
	}
	else if (relStorageMgr == PersistentFileSysRelStorageMgr_AppendOnly)
	{
		int64 mirrorAppendOnlyNewEof;

		mirrorAppendOnlyNewEof = DatumGetInt64(values[Anum_gp_persistent_relation_node_mirror_append_only_new_eof - 1]);

		Assert(mirrorLossEof <= mirrorAppendOnlyNewEof);

		replaces[Anum_gp_persistent_relation_node_mirror_append_only_loss_eof - 1] = true;
		newValues[Anum_gp_persistent_relation_node_mirror_append_only_loss_eof - 1] =
										Int64GetDatum(mirrorLossEof);
	}

	PersistentFileSysObj_ReplaceTuple(
								PersistentFsObjType_RelationFile,
								persistentTid,
								tupleCopy,
								newValues,
								replaces,
								flushToXLog);

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

}

bool PersistentFileSysObj_ScanForRelation(
	RelFileNode 		*relFileNode,
				/* The tablespace, database, and relation OIDs for the create. */

	int32				segmentFileNum,

	ItemPointer			persistentTid,
				/* Resulting TID of the gp_persistent_rel_files tuple for the relation. */

	int64				*persistentSerialNum)
				/* Resulting serial number for the relation.  Distinquishes the uses of the tuple. */
{
	READ_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjData *fileSysObjData;
	PersistentFileSysObjSharedData	*fileSysObjSharedData;

	PersistentStoreScan storeScan;

	Datum values[Natts_gp_persistent_relation_node];

	bool found;

	if(RelFileNode_IsEmpty(relFileNode))
		elog(ERROR, "Invalid RelFileNode (0,0,0)");

	if (Persistent_BeforePersistenceWork())
	{
		elog(ERROR,
		     "Cannot scan for persistent relation %u/%u/%u, segment file #%d because we are before persistence work",
			 relFileNode->spcNode,
			 relFileNode->dbNode,
			 relFileNode->relNode,
			 segmentFileNum);
	}

	PersistentFileSysObj_VerifyInitScan();

	PersistentFileSysObj_GetDataPtrs(
								PersistentFsObjType_RelationFile,
								&fileSysObjData,
								&fileSysObjSharedData);

	found = false;
	MemSet(persistentTid, 0, sizeof(ItemPointerData));
	*persistentSerialNum = 0;

	READ_PERSISTENT_STATE_ORDERED_LOCK;

	PersistentStore_BeginScan(
						&fileSysObjData->storeData,
						&fileSysObjSharedData->storeSharedData,
						&storeScan);

	while (PersistentStore_GetNext(
							&storeScan,
							values,
							persistentTid,
							persistentSerialNum))
	{
		RelFileNode 					candidateRelFileNode;
		int32 							candidateSegmentFileNum;

		PersistentFileSysRelStorageMgr	relationStorageManager;
		PersistentFileSysState			persistentState;
		int64							createMirrorDataLossTrackingSessionNum;
		MirroredObjectExistenceState	mirrorExistenceState;
		MirroredRelDataSynchronizationState mirrorDataSynchronizationState;
		bool							mirrorBufpoolMarkedForScanIncrementalResync;
		int64							mirrorBufpoolResyncChangedPageCount;
		XLogRecPtr						mirrorBufpoolResyncCkptLoc;
		BlockNumber 					mirrorBufpoolResyncCkptBlockNum;
		int64							mirrorAppendOnlyLossEof;
		int64							mirrorAppendOnlyNewEof;
		PersistentFileSysRelBufpoolKind relBufpoolKind;
		TransactionId					parentXid;
		int64							serialNum;

		GpPersistentRelationNode_GetValues(
										values,
										&candidateRelFileNode.spcNode,
										&candidateRelFileNode.dbNode,
										&candidateRelFileNode.relNode,
										&candidateSegmentFileNum,
										&relationStorageManager,
										&persistentState,
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

		if (persistentState != PersistentFileSysState_BulkLoadCreatePending &&
			persistentState != PersistentFileSysState_CreatePending &&
			persistentState != PersistentFileSysState_Created)
			continue;

		if (RelFileNodeEquals(candidateRelFileNode, *relFileNode) &&
			candidateSegmentFileNum == segmentFileNum)
		{
			found = true;
			break;
		}

		READ_PERSISTENT_STATE_ORDERED_UNLOCK;

		READ_PERSISTENT_STATE_ORDERED_LOCK;

	}

	READ_PERSISTENT_STATE_ORDERED_UNLOCK;

	PersistentStore_EndScan(&storeScan);

	if (found && Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
		     "Scan found persistent relation %u/%u/%u, segment file #%d with serial number " INT64_FORMAT " at TID %s",
			 relFileNode->spcNode,
			 relFileNode->dbNode,
			 relFileNode->relNode,
			 segmentFileNum,
			 *persistentSerialNum,
			 ItemPointerToString(persistentTid));

	return found;
}

static void PersistentFileSysObj_StartupIntegrityCheckRelation(void)
{
	PersistentFileSysObjData *fileSysObjData;
	PersistentFileSysObjSharedData	*fileSysObjSharedData;

	PersistentStoreScan storeScan;

	Datum values[Natts_gp_persistent_relation_node];

	ItemPointerData persistentTid;
	int64			persistentSerialNum;

	PersistentFileSysObj_GetDataPtrs(
								PersistentFsObjType_RelationFile,
								&fileSysObjData,
								&fileSysObjSharedData);


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
		RelFileNode 					relFileNode;
		int32 							segmentFileNum;

		PersistentFileSysRelStorageMgr	relationStorageManager;
		PersistentFileSysState			persistentState;
		int64							createMirrorDataLossTrackingSessionNum;
		MirroredObjectExistenceState	mirrorExistenceState;
		MirroredRelDataSynchronizationState mirrorDataSynchronizationState;
		bool							mirrorBufpoolMarkedForScanIncrementalResync;
		int64							mirrorBufpoolResyncChangedPageCount;
		XLogRecPtr						mirrorBufpoolResyncCkptLoc;
		BlockNumber 					mirrorBufpoolResyncCkptBlockNum;
		int64							mirrorAppendOnlyLossEof;
		int64							mirrorAppendOnlyNewEof;
		PersistentFileSysRelBufpoolKind relBufpoolKind;
		TransactionId					parentXid;
		int64							serialNum;

		DbDirNode dbDirNode;

		GpPersistentRelationNode_GetValues(
										values,
										&relFileNode.spcNode,
										&relFileNode.dbNode,
										&relFileNode.relNode,
										&segmentFileNum,
										&relationStorageManager,
										&persistentState,
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

		if (persistentState == PersistentFileSysState_Free)
			continue;

		/*
		 * Verify this relation has a database directory parent.
		 */
		dbDirNode.tablespace = relFileNode.spcNode;
		dbDirNode.database = relFileNode.dbNode;

		if (dbDirNode.database == 0)
		{
			/*
			 * We don't represent the shared database directory in
			 * gp_persistent_database_node since it cannot be dropped.
			 */
			Assert(dbDirNode.tablespace == GLOBALTABLESPACE_OID);
			continue;
		}

		if (!PersistentDatabase_DbDirExistsUnderLock(&dbDirNode))
		{
			elog(LOG,
				 "Database directory not found for relation %u/%u/%u",
				 relFileNode.spcNode,
				 relFileNode.dbNode,
				 relFileNode.relNode);
		}
	}

	PersistentStore_EndScan(&storeScan);
}

static void PersistentFileSysObj_StartupIntegrityCheckPrintDbDirs(void)
{
	DbDirNode dbDirNode;
	PersistentFileSysState state;

	ItemPointerData persistentTid;
	int64 persistentSerialNum;

	PersistentDatabase_DirIterateInit();
	while (PersistentDatabase_DirIterateNext(
									&dbDirNode,
									&state,
									&persistentTid,
									&persistentSerialNum))
	{
		elog(PersistentRecovery_DebugPrintLevel(),
			 "STARTUP INTEGRITY: Database database %u/%u in state '%s'",
			 dbDirNode.tablespace,
			 dbDirNode.database,
			 PersistentFileSysObjState_Name(state));
	}
}

void PersistentFileSysObj_StartupIntegrityCheck(void)
{
	/*
	 * Show the existing database directories.
	 */
	if (Debug_persistent_recovery_print)
	{
		PersistentFileSysObj_StartupIntegrityCheckPrintDbDirs();
	}

	/*Verify each relation has a database directory parent.*/
	PersistentFileSysObj_StartupIntegrityCheckRelation();

	/*
	 * Show the existing tablespace directories.
	 */
	if (Debug_persistent_recovery_print)
	{
//		PersistentFileSysObj_StartupIntegrityCheckPrintTablespaceDirs();
	}

}

static Size PersistentFileSysObj_SharedDataSize(void)
{
	return MAXALIGN(sizeof(PersistentFileSysObjPrivateSharedMemory));
}

/*
 * Return the required shared-memory size for this module.
 */
Size PersistentFileSysObj_ShmemSize(void)
{
	Size size = 0;

	/* The shared-memory structure. */
	size = add_size(size, PersistentFileSysObj_SharedDataSize());

	return size;
}

/*
 * Initialize the shared-memory for this module.
 */
void PersistentFileSysObj_ShmemInit(void)
{
	bool found;

	/* Create the shared-memory structure. */
	persistentFileSysObjPrivateSharedData =
		(PersistentFileSysObjPrivateSharedMemory *)
						ShmemInitStruct("Persist File-Sys Data",
										PersistentFileSysObj_SharedDataSize(),
										&found);

	if (!found)
	{
		persistentFileSysObjPrivateSharedData->needInitScan = true;
	}
}

/* 
 * Check for inconsistencies in global sequence number and then update to the right value if necessary 
 * WARNING : This is a special routine called during repair only under guc gp_persistent_repair_global_sequence
 */
void 
PersistentFileSysObj_DoGlobalSequenceScan(void)
{

	PersistentFsObjType             fsObjType;
	int64                   	globalSequenceNum;

	PersistentFileSysObjData        *fileSysObjData;
	PersistentFileSysObjSharedData  *fileSysObjSharedData;

	Assert(!persistentFileSysObjPrivateSharedData->needInitScan);

	if (gp_persistent_repair_global_sequence)
	{
	
		for (fsObjType = PersistentFsObjType_First;
			fsObjType <= PersistentFsObjType_Last;
			fsObjType++)
		{
                 	PersistentFileSysObj_GetDataPtrs(
                                                                         fsObjType,
                                                                         &fileSysObjData,
                                                                         &fileSysObjSharedData);

			globalSequenceNum = GlobalSequence_Current(fileSysObjData->storeData.gpGlobalSequence);

			if (globalSequenceNum < fileSysObjSharedData->storeSharedData.maxInUseSerialNum)
			{
				/*
				 * We seem to have a corruption problem.
				 *
				 * Use the gp_persistent_repair_global_sequence GUC to get the system up.
				 */
				GlobalSequence_Set(fileSysObjData->storeData.gpGlobalSequence, fileSysObjSharedData->storeSharedData.maxInUseSerialNum);

				elog(LOG, "Repaired global sequence number " INT64_FORMAT " so use scanned maximum value " INT64_FORMAT " ('%s')",
                                 	globalSequenceNum,
                                 	fileSysObjSharedData->storeSharedData.maxInUseSerialNum,
                                 	fileSysObjData->storeData.tableName);
			}

		}
	}
}
