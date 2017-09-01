/*-------------------------------------------------------------------------
 *
 * gp_persistent.c
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/catalog/gp_persistent.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "utils/palloc.h"
#include "storage/fd.h"

#include "catalog/catalog.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_database.h"
#include "catalog/gp_persistent.h"
#include "cdb/cdbsharedoidsearch.h"
#include "storage/itemptr.h"
#include "storage/shmem.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/transam.h"
#include "utils/guc.h"
#include "storage/smgr.h"
#include "storage/ipc.h"
#include "utils/builtins.h"

void GpPersistentRelationNode_GetRelationInfo(
	char		relkind,			/* see RELKIND_xxx constants */
	char		relstorage,			/* see RELSTORAGE_xxx constants */
	Oid			relam,				/* index access method; 0 if not an index */

	PersistentFileSysRelStorageMgr			*relationStorageManager,
	PersistentFileSysRelBufpoolKind			*relationBufpoolKind)
{
	// Assume.
	*relationStorageManager = PersistentFileSysRelStorageMgr_BufferPool;	
	*relationBufpoolKind = PersistentFileSysRelBufpoolKind_None;

	switch(relkind)
	{
	case RELKIND_INDEX:				/* secondary index */
		if (relam == BTREE_AM_OID)
		{
			*relationBufpoolKind = PersistentFileSysRelBufpoolKind_Btree;
		}
		else if (relam == BITMAP_AM_OID)
		{
			*relationBufpoolKind = PersistentFileSysRelBufpoolKind_BitMap;
		}
		else
		{
			*relationBufpoolKind = PersistentFileSysRelBufpoolKind_UnknownIndex;
		}
		break;

	case RELKIND_RELATION:			/* ordinary cataloged heap or Append-Only */
		if (relstorage_is_ao(relstorage))
		{
			*relationStorageManager = PersistentFileSysRelStorageMgr_AppendOnly;	
			Assert(*relationBufpoolKind == PersistentFileSysRelBufpoolKind_None);
		}
		else if (relstorage == 'h')
		{
			*relationBufpoolKind = PersistentFileSysRelBufpoolKind_Heap;
		}
		else
		{
			*relationBufpoolKind = PersistentFileSysRelBufpoolKind_UnknownRelStorage;
		}
		break;

	case RELKIND_SEQUENCE:			/* SEQUENCE relation */
		*relationBufpoolKind = PersistentFileSysRelBufpoolKind_Sequence;
		break;

	case RELKIND_TOASTVALUE:		/* moved off huge values */
		*relationBufpoolKind = PersistentFileSysRelBufpoolKind_Toast;
		break;

	case RELKIND_AOSEGMENTS:		/* AO segment files and eof's */
		*relationBufpoolKind = PersistentFileSysRelBufpoolKind_AppendOnlySeginfo;
		break;

	case RELKIND_AOBLOCKDIR:		/* AO block directory */
		*relationBufpoolKind = PersistentFileSysRelBufpoolKind_AppendOnlyBlockDirectory;
		break;
	
	case RELKIND_AOVISIMAP:		/* AO visibility map */
		*relationBufpoolKind = PersistentFileSysRelBufpoolKind_AppendOnlyVisibilityMap;
		break;

	case RELKIND_UNCATALOGED:		/* temporary heap */
		*relationBufpoolKind = PersistentFileSysRelBufpoolKind_UncatalogedHeap;
		break;

	case RELKIND_VIEW:				/* view */
	case RELKIND_COMPOSITE_TYPE:	/* composite type */
		*relationStorageManager = PersistentFileSysRelStorageMgr_None;
		break;

	default:
		*relationBufpoolKind = PersistentFileSysRelBufpoolKind_UnknownRelKind;
		break;

	}
}

void GpPersistentRelationNode_GetValues(
	Datum									*values,

	Oid 									*tablespaceOid,
	Oid 									*databaseOid,
	Oid 									*relfilenodeOid,
	int32									*segmentFileNum,
	PersistentFileSysRelStorageMgr			*relationStorageManager,
	PersistentFileSysState					*persistentState,
	int64									*createMirrorDataLossTrackingSessionNum,
	MirroredObjectExistenceState			*mirrorExistenceState,
	MirroredRelDataSynchronizationState 	*mirrorDataSynchronizationState,
	bool									*mirrorBufpoolMarkedForScanIncrementalResync,
	int64									*mirrorBufpoolResyncChangedPageCount,
	XLogRecPtr								*mirrorBufpoolResyncCkptLoc,
	BlockNumber								*mirrorBufpoolResyncCkptBlockNum,
	int64									*mirrorAppendOnlyLossEof,
	int64									*mirrorAppendOnlyNewEof,
	PersistentFileSysRelBufpoolKind 		*relBufpoolKind,
	TransactionId							*parentXid,
	int64									*persistentSerialNum)
{
	*tablespaceOid = DatumGetObjectId(values[Anum_gp_persistent_relation_node_tablespace_oid - 1]);

	*databaseOid = DatumGetObjectId(values[Anum_gp_persistent_relation_node_database_oid - 1]);

	*relfilenodeOid = DatumGetObjectId(values[Anum_gp_persistent_relation_node_relfilenode_oid - 1]);

	*segmentFileNum = DatumGetInt32(values[Anum_gp_persistent_relation_node_segment_file_num - 1]);

	*relationStorageManager = (PersistentFileSysRelStorageMgr)DatumGetInt16(values[Anum_gp_persistent_relation_node_relation_storage_manager - 1]);

	*persistentState = (PersistentFileSysState)DatumGetInt16(values[Anum_gp_persistent_relation_node_persistent_state - 1]);

	*createMirrorDataLossTrackingSessionNum = DatumGetInt64(values[Anum_gp_persistent_relation_node_create_mirror_data_loss_tracking_session_num - 1]);

	*mirrorExistenceState = (MirroredObjectExistenceState)DatumGetInt16(values[Anum_gp_persistent_relation_node_mirror_existence_state - 1]);

	*mirrorDataSynchronizationState = (MirroredRelDataSynchronizationState)DatumGetInt16(values[Anum_gp_persistent_relation_node_mirror_data_synchronization_state - 1]);

	*mirrorBufpoolMarkedForScanIncrementalResync = DatumGetBool(values[Anum_gp_persistent_relation_node_mirror_bufpool_marked_for_scan_incremental_resync - 1]);

	*mirrorBufpoolResyncChangedPageCount = DatumGetInt64(values[Anum_gp_persistent_relation_node_mirror_bufpool_resync_changed_page_count - 1]);

	*mirrorBufpoolResyncCkptLoc = *((XLogRecPtr*) DatumGetPointer(values[Anum_gp_persistent_relation_node_mirror_bufpool_resync_ckpt_loc - 1]));

	*mirrorBufpoolResyncCkptBlockNum = (BlockNumber)DatumGetInt32(values[Anum_gp_persistent_relation_node_mirror_bufpool_resync_ckpt_block_num - 1]);

	*mirrorAppendOnlyLossEof = DatumGetInt64(values[Anum_gp_persistent_relation_node_mirror_append_only_loss_eof - 1]);

	*mirrorAppendOnlyNewEof = DatumGetInt64(values[Anum_gp_persistent_relation_node_mirror_append_only_new_eof - 1]);

	*relBufpoolKind = (PersistentFileSysRelBufpoolKind)DatumGetInt32(values[Anum_gp_persistent_relation_node_relation_bufpool_kind - 1]);

	*parentXid = (TransactionId)DatumGetInt32(values[Anum_gp_persistent_relation_node_parent_xid - 1]);

	*persistentSerialNum = DatumGetInt64(values[Anum_gp_persistent_relation_node_persistent_serial_num - 1]);
}

void GpPersistentRelationNode_SetDatumValues(
	Datum							*values,

	Oid 							tablespaceOid,
	Oid 							databaseOid,
	Oid 							relfilenodeOid,
	int32							segmentFileNum,
	PersistentFileSysRelStorageMgr	relationStorageManager,
	PersistentFileSysState			persistentState,
	int64							createMirrorDataLossTrackingSessionNum,
	MirroredObjectExistenceState	mirrorExistenceState,
	MirroredRelDataSynchronizationState mirrorDataSynchronizationState,
	bool							mirrorBufpoolMarkedForScanIncrementalResync,
	int64							mirrorBufpoolResyncChangedPageCount,
	XLogRecPtr						*mirrorBufpoolResyncCkptLoc,
	BlockNumber						mirrorBufpoolResyncCkptBlockNum,
	int64							mirrorAppendOnlyLossEof,
	int64							mirrorAppendOnlyNewEof,
	PersistentFileSysRelBufpoolKind relBufpoolKind,
	TransactionId					parentXid,
	int64							persistentSerialNum)
{
	if (persistentState != PersistentFileSysState_Free && !PersistentFileSysRelStorageMgr_IsValid(relationStorageManager))
		elog(ERROR, "Invalid value for relation storage manager (%d)",
		     relationStorageManager);

	values[Anum_gp_persistent_relation_node_tablespace_oid - 1] = 
									ObjectIdGetDatum(tablespaceOid);
	values[Anum_gp_persistent_relation_node_database_oid - 1] = 
									ObjectIdGetDatum(databaseOid);
	values[Anum_gp_persistent_relation_node_relfilenode_oid - 1] = 
									ObjectIdGetDatum(relfilenodeOid);

	values[Anum_gp_persistent_relation_node_segment_file_num - 1] = 
									Int32GetDatum(segmentFileNum);

	values[Anum_gp_persistent_relation_node_relation_storage_manager - 1] = 
									Int16GetDatum(relationStorageManager);

	values[Anum_gp_persistent_relation_node_persistent_state - 1] = 
									Int16GetDatum(persistentState);

	values[Anum_gp_persistent_relation_node_create_mirror_data_loss_tracking_session_num - 1] = 
									Int64GetDatum(createMirrorDataLossTrackingSessionNum);

	values[Anum_gp_persistent_relation_node_mirror_existence_state - 1] = 
									Int16GetDatum(mirrorExistenceState);

	values[Anum_gp_persistent_relation_node_mirror_data_synchronization_state - 1] = 
									Int16GetDatum(mirrorDataSynchronizationState);

	values[Anum_gp_persistent_relation_node_mirror_bufpool_marked_for_scan_incremental_resync - 1] = 
									BoolGetDatum(mirrorBufpoolMarkedForScanIncrementalResync);

	values[Anum_gp_persistent_relation_node_mirror_bufpool_resync_changed_page_count - 1] = 
									Int64GetDatum(mirrorBufpoolResyncChangedPageCount);

	values[Anum_gp_persistent_relation_node_mirror_bufpool_resync_ckpt_loc - 1] =
									PointerGetDatum(mirrorBufpoolResyncCkptLoc);

	values[Anum_gp_persistent_relation_node_mirror_bufpool_resync_ckpt_block_num - 1] = 
									Int32GetDatum(mirrorBufpoolResyncCkptBlockNum);

	values[Anum_gp_persistent_relation_node_mirror_append_only_loss_eof - 1] = 
									Int64GetDatum(mirrorAppendOnlyLossEof);

	values[Anum_gp_persistent_relation_node_mirror_append_only_new_eof - 1] = 
									Int64GetDatum(mirrorAppendOnlyNewEof);

	values[Anum_gp_persistent_relation_node_relation_bufpool_kind - 1] = 
									Int32GetDatum((int32)relBufpoolKind);

	values[Anum_gp_persistent_relation_node_parent_xid - 1] = 
									Int32GetDatum(parentXid);

	values[Anum_gp_persistent_relation_node_persistent_serial_num - 1] = 
									Int64GetDatum(persistentSerialNum);
}

void GpPersistentDatabaseNode_GetValues(
	Datum							*values,

	Oid 							*tablespaceOid,
	Oid 							*databaseOid,
	PersistentFileSysState			*persistentState,
	int64							*createMirrorDataLossTrackingSessionNum,
	MirroredObjectExistenceState	*mirrorExistenceState,
	int32							*reserved,
	TransactionId					*parentXid,
	int64							*persistentSerialNum)
{
    *tablespaceOid = DatumGetObjectId(values[Anum_gp_persistent_database_node_tablespace_oid - 1]);

    *databaseOid = DatumGetObjectId(values[Anum_gp_persistent_database_node_database_oid - 1]);

    *persistentState = DatumGetInt16(values[Anum_gp_persistent_database_node_persistent_state - 1]);

    *createMirrorDataLossTrackingSessionNum = DatumGetInt64(values[Anum_gp_persistent_database_node_create_mirror_data_loss_tracking_session_num - 1]);

    *mirrorExistenceState = DatumGetInt16(values[Anum_gp_persistent_database_node_mirror_existence_state - 1]);

	*reserved = DatumGetInt32(values[Anum_gp_persistent_database_node_reserved - 1]);

	*parentXid = (TransactionId)DatumGetInt32(values[Anum_gp_persistent_database_node_parent_xid - 1]);

    *persistentSerialNum = DatumGetInt64(values[Anum_gp_persistent_database_node_persistent_serial_num - 1]);
}

void GpPersistentDatabaseNode_SetDatumValues(
	Datum							*values,

	Oid 							tablespaceOid,
	Oid 							databaseOid,
	PersistentFileSysState			persistentState,
	int64							createMirrorDataLossTrackingSessionNum,
	MirroredObjectExistenceState	mirrorExistenceState,
	int32							reserved,
	TransactionId					parentXid,
	int64							persistentSerialNum)
{
	values[Anum_gp_persistent_database_node_tablespace_oid - 1] = 
									ObjectIdGetDatum(tablespaceOid);
	values[Anum_gp_persistent_database_node_database_oid - 1] = 
									ObjectIdGetDatum(databaseOid);

	values[Anum_gp_persistent_database_node_persistent_state - 1] = 
									Int16GetDatum(persistentState);

	values[Anum_gp_persistent_database_node_create_mirror_data_loss_tracking_session_num - 1] = 
									Int64GetDatum(createMirrorDataLossTrackingSessionNum);

	values[Anum_gp_persistent_database_node_mirror_existence_state - 1] = 
									Int16GetDatum(mirrorExistenceState);

	values[Anum_gp_persistent_database_node_reserved - 1] = 
									Int32GetDatum(reserved);

	values[Anum_gp_persistent_database_node_parent_xid - 1] = 
									Int32GetDatum(parentXid);

	values[Anum_gp_persistent_database_node_persistent_serial_num - 1] = 
									Int64GetDatum(persistentSerialNum);
}

void GpPersistentTablespaceNode_GetValues(
	Datum							*values,

	Oid 							*filespaceOid,
	Oid 							*tablespaceOid,
	PersistentFileSysState			*persistentState,
	int64							*createMirrorDataLossTrackingSessionNum,
	MirroredObjectExistenceState	*mirrorExistenceState,
	int32							*reserved,
	TransactionId					*parentXid,
	int64							*persistentSerialNum)
{
    *filespaceOid = DatumGetObjectId(values[Anum_gp_persistent_tablespace_node_filespace_oid - 1]);

    *tablespaceOid = DatumGetObjectId(values[Anum_gp_persistent_tablespace_node_tablespace_oid - 1]);

    *persistentState = DatumGetInt16(values[Anum_gp_persistent_tablespace_node_persistent_state - 1]);

    *createMirrorDataLossTrackingSessionNum = DatumGetInt64(values[Anum_gp_persistent_tablespace_node_create_mirror_data_loss_tracking_session_num - 1]);

    *mirrorExistenceState = DatumGetInt16(values[Anum_gp_persistent_tablespace_node_mirror_existence_state - 1]);

	*reserved = DatumGetInt32(values[Anum_gp_persistent_tablespace_node_reserved - 1]);

	*parentXid = (TransactionId)DatumGetInt32(values[Anum_gp_persistent_tablespace_node_parent_xid - 1]);

    *persistentSerialNum = DatumGetInt64(values[Anum_gp_persistent_tablespace_node_persistent_serial_num - 1]);
}

void GpPersistentTablespaceNode_SetDatumValues(
	Datum							*values,

	Oid 							filespaceOid,
	Oid 							tablespaceOid,
	PersistentFileSysState			persistentState,
	int64							createMirrorDataLossTrackingSessionNum,
	MirroredObjectExistenceState	mirrorExistenceState,
	int32							reserved,
	TransactionId					parentXid,
	int64							persistentSerialNum)
{
	values[Anum_gp_persistent_tablespace_node_filespace_oid - 1] = 
									ObjectIdGetDatum(filespaceOid);

	values[Anum_gp_persistent_tablespace_node_tablespace_oid - 1] = 
									ObjectIdGetDatum(tablespaceOid);

	values[Anum_gp_persistent_tablespace_node_persistent_state - 1] = 
									Int16GetDatum(persistentState);

	values[Anum_gp_persistent_tablespace_node_create_mirror_data_loss_tracking_session_num - 1] = 
									Int64GetDatum(createMirrorDataLossTrackingSessionNum);

	values[Anum_gp_persistent_tablespace_node_mirror_existence_state - 1] = 
									Int16GetDatum(mirrorExistenceState);

	values[Anum_gp_persistent_tablespace_node_reserved - 1] = 
									Int32GetDatum(reserved);

	values[Anum_gp_persistent_tablespace_node_parent_xid - 1] = 
									Int32GetDatum(parentXid);

	values[Anum_gp_persistent_tablespace_node_persistent_serial_num - 1] = 
									Int64GetDatum(persistentSerialNum);
}

void GpPersistentFilespaceNode_GetValues(
	Datum							*values,

	Oid 							*filespaceOid,
	int16							*dbId1,
	char							locationBlankPadded1[FilespaceLocationBlankPaddedWithNullTermLen],
	int16							*dbId2,
	char							locationBlankPadded2[FilespaceLocationBlankPaddedWithNullTermLen],
	PersistentFileSysState			*persistentState,
	int64							*createMirrorDataLossTrackingSessionNum,
	MirroredObjectExistenceState	*mirrorExistenceState,
	int32							*reserved,
	TransactionId					*parentXid,
	int64							*persistentSerialNum)
{
	char *locationPtr;
	int locationLen;

    *filespaceOid = DatumGetObjectId(values[Anum_gp_persistent_filespace_node_filespace_oid - 1]);

    *dbId1 = DatumGetInt16(values[Anum_gp_persistent_filespace_node_db_id_1 - 1]);

	locationPtr = TextDatumGetCString(values[Anum_gp_persistent_filespace_node_location_1 - 1]);;
	locationLen = strlen(locationPtr);
	if (locationLen != FilespaceLocationBlankPaddedWithNullTermLen - 1)
		elog(ERROR, "Expected filespace location 1 to be %d characters and found %d",
			 FilespaceLocationBlankPaddedWithNullTermLen - 1,
			 locationLen);
			 
	memcpy(locationBlankPadded1, locationPtr, FilespaceLocationBlankPaddedWithNullTermLen);

    *dbId2 = DatumGetInt16(values[Anum_gp_persistent_filespace_node_db_id_2 - 1]);

	locationPtr = TextDatumGetCString(values[Anum_gp_persistent_filespace_node_location_2 - 1]);
	locationLen = strlen(locationPtr);
	if (locationLen != FilespaceLocationBlankPaddedWithNullTermLen - 1)
		elog(ERROR, "Expected filespace location 2 to be %d characters and found %d",
			 FilespaceLocationBlankPaddedWithNullTermLen - 1,
			 locationLen);
			 
	memcpy(locationBlankPadded2, locationPtr, FilespaceLocationBlankPaddedWithNullTermLen);

    *persistentState = DatumGetInt16(values[Anum_gp_persistent_filespace_node_persistent_state - 1]);

    *createMirrorDataLossTrackingSessionNum = DatumGetInt64(values[Anum_gp_persistent_filespace_node_create_mirror_data_loss_tracking_session_num - 1]);

    *mirrorExistenceState = DatumGetInt16(values[Anum_gp_persistent_filespace_node_mirror_existence_state - 1]);

	*reserved = DatumGetInt32(values[Anum_gp_persistent_filespace_node_reserved - 1]);

	*parentXid = (TransactionId)DatumGetInt32(values[Anum_gp_persistent_filespace_node_parent_xid - 1]);

    *persistentSerialNum = DatumGetInt64(values[Anum_gp_persistent_filespace_node_persistent_serial_num - 1]);
}

void GpPersistentFilespaceNode_SetDatumValues(
	Datum							*values,

	Oid 							filespaceOid,
	int16							dbId1,
	char							locationBlankPadded1[FilespaceLocationBlankPaddedWithNullTermLen],
	int16							dbId2,
	char							locationBlankPadded2[FilespaceLocationBlankPaddedWithNullTermLen],
	PersistentFileSysState			persistentState,
	int64							createMirrorDataLossTrackingSessionNum,
	MirroredObjectExistenceState	mirrorExistenceState,
	int32							reserved,
	TransactionId					parentXid,
	int64							persistentSerialNum)
{
	int locationLen;

	locationLen = strlen(locationBlankPadded1);
	if (locationLen != FilespaceLocationBlankPaddedWithNullTermLen - 1)
		elog(ERROR, "Expected filespace location 1 to be %d characters and found %d",
			 FilespaceLocationBlankPaddedWithNullTermLen - 1,
			 locationLen);

	locationLen = strlen(locationBlankPadded2);
	if (locationLen != FilespaceLocationBlankPaddedWithNullTermLen - 1)
		elog(ERROR, "Expected filespace location 2 to be %d characters and found %d",
			 FilespaceLocationBlankPaddedWithNullTermLen - 1,
			 locationLen);

	values[Anum_gp_persistent_filespace_node_filespace_oid - 1] = 
									ObjectIdGetDatum(filespaceOid);

	values[Anum_gp_persistent_filespace_node_db_id_1 - 1] = 
									Int16GetDatum(dbId1);

	values[Anum_gp_persistent_filespace_node_location_1 - 1] =
									CStringGetTextDatum(locationBlankPadded1);

	values[Anum_gp_persistent_filespace_node_db_id_2 - 1] = 
									Int16GetDatum(dbId2);

	values[Anum_gp_persistent_filespace_node_location_2 - 1] =
									CStringGetTextDatum(locationBlankPadded2);

	values[Anum_gp_persistent_filespace_node_persistent_state - 1] = 
									Int16GetDatum(persistentState);

	values[Anum_gp_persistent_filespace_node_create_mirror_data_loss_tracking_session_num - 1] = 
									Int64GetDatum(createMirrorDataLossTrackingSessionNum);

	values[Anum_gp_persistent_filespace_node_mirror_existence_state - 1] = 
									Int16GetDatum(mirrorExistenceState);

	values[Anum_gp_persistent_filespace_node_reserved - 1] = 
									Int32GetDatum(reserved);

	values[Anum_gp_persistent_filespace_node_parent_xid - 1] = 
									Int32GetDatum(parentXid);

	values[Anum_gp_persistent_filespace_node_persistent_serial_num - 1] = 
									Int64GetDatum(persistentSerialNum);
}

void GpPersistent_GetCommonValues(
	PersistentFsObjType 			fsObjType,
	Datum							*values,

	PersistentFileSysObjName		*fsObjName,
	PersistentFileSysState			*persistentState,
	MirroredObjectExistenceState	*mirrorExistenceState,
	TransactionId					*parentXid,
	int64							*persistentSerialNum)
{
	int64 createMirrorDataLossTrackingSessionNum;
	int32 reserved;

	switch (fsObjType)
	{
	case PersistentFsObjType_RelationFile:
		{
			RelFileNode 					relFileNode;
			int32 							segmentFileNum;

			PersistentFileSysRelStorageMgr	relationStorageManager;
			MirroredRelDataSynchronizationState mirrorDataSynchronizationState;
			bool							mirrorBufpoolMarkedForScanIncrementalResync;
			int64							mirrorBufpoolResyncChangedPageCount;
			XLogRecPtr						mirrorBufpoolResyncCkptLoc;
			BlockNumber 					mirrorBufpoolResyncCkptBlockNum;
			int64							mirrorAppendOnlyLossEof;
			int64							mirrorAppendOnlyNewEof;
			PersistentFileSysRelBufpoolKind relBufpoolKind;

			GpPersistentRelationNode_GetValues(
											values,
											&relFileNode.spcNode,
											&relFileNode.dbNode,
											&relFileNode.relNode,
											&segmentFileNum,
											&relationStorageManager,
											persistentState,
											&createMirrorDataLossTrackingSessionNum,
											mirrorExistenceState,
											&mirrorDataSynchronizationState,
											&mirrorBufpoolMarkedForScanIncrementalResync,
											&mirrorBufpoolResyncChangedPageCount,
											&mirrorBufpoolResyncCkptLoc,
											&mirrorBufpoolResyncCkptBlockNum,
											&mirrorAppendOnlyLossEof,
											&mirrorAppendOnlyNewEof,
											&relBufpoolKind,
											parentXid,
											persistentSerialNum);

			PersistentFileSysObjName_SetRelationFile(
												fsObjName,
												&relFileNode,
												segmentFileNum);
			
		}
		break;

	case PersistentFsObjType_DatabaseDir:
		{
			DbDirNode dbDirNode;

			GpPersistentDatabaseNode_GetValues(
									values,
									&dbDirNode.tablespace,
									&dbDirNode.database,
									persistentState,
									&createMirrorDataLossTrackingSessionNum,
									mirrorExistenceState,
									&reserved,
									parentXid,
									persistentSerialNum);

			PersistentFileSysObjName_SetDatabaseDir(
											fsObjName,
											dbDirNode.tablespace,
											dbDirNode.database);
		}
		break;

	case PersistentFsObjType_TablespaceDir:
		{
			Oid tablespaceOid;
			Oid filespaceOid;

			GpPersistentTablespaceNode_GetValues(
											values,
											&filespaceOid,
											&tablespaceOid,
											persistentState,
											&createMirrorDataLossTrackingSessionNum,
											mirrorExistenceState,
											&reserved,
											parentXid,
											persistentSerialNum);

			PersistentFileSysObjName_SetTablespaceDir(
												fsObjName, 
												tablespaceOid);
		}
	break;
	
	case PersistentFsObjType_FilespaceDir:
		{
			Oid filespaceOid;
			int16 dbId1;
			char locationBlankPadded1[FilespaceLocationBlankPaddedWithNullTermLen];
			int16 dbId2;
			char locationBlankPadded2[FilespaceLocationBlankPaddedWithNullTermLen];

			GpPersistentFilespaceNode_GetValues(
											values,
											&filespaceOid,
											&dbId1,
											locationBlankPadded1,
											&dbId2,
											locationBlankPadded2,
											persistentState,
											&createMirrorDataLossTrackingSessionNum,
											mirrorExistenceState,
											&reserved,
											parentXid,
											persistentSerialNum);

			PersistentFileSysObjName_SetFilespaceDir(
												fsObjName, 
												filespaceOid);
		}
	break;

	default:
		elog(ERROR, "Unexpected persistent file-system object type: %d",
			 fsObjType);
	}
		
}

void GpRelationNode_GetValues(
	Datum							*values,
	Oid 							*tablespaceOid,
	Oid 							*relfilenodeOid,
	int32							*segmentFileNum,
	int64							*createMirrorDataLossTrackingSessionNum,
	ItemPointer		 				persistentTid,
	int64							*persistentSerialNum)
{
	*tablespaceOid = DatumGetObjectId(values[Anum_gp_relation_node_tablespace_oid - 1]);

	*relfilenodeOid = DatumGetObjectId(values[Anum_gp_relation_node_relfilenode_oid - 1]);

	*segmentFileNum = DatumGetInt32(values[Anum_gp_relation_node_segment_file_num - 1]);

	*createMirrorDataLossTrackingSessionNum = DatumGetInt64(values[Anum_gp_relation_node_create_mirror_data_loss_tracking_session_num - 1]);

	*persistentTid = *((ItemPointer) DatumGetPointer(values[Anum_gp_relation_node_persistent_tid - 1]));

	*persistentSerialNum = DatumGetInt64(values[Anum_gp_relation_node_persistent_serial_num - 1]);

}

void GpRelationNode_SetDatumValues(
	Datum							*values,
	Oid 							tablespaceOid,
	Oid 							relfilenodeOid,
	int32							segmentFileNum,
	int64							createMirrorDataLossTrackingSessionNum,
	ItemPointer		 				persistentTid,
	int64							persistentSerialNum)
{
	values[Anum_gp_relation_node_tablespace_oid - 1] =
		ObjectIdGetDatum(tablespaceOid);

	values[Anum_gp_relation_node_relfilenode_oid - 1] = 
									ObjectIdGetDatum(relfilenodeOid);

	values[Anum_gp_relation_node_segment_file_num - 1] = 
									Int32GetDatum(segmentFileNum);

	values[Anum_gp_relation_node_create_mirror_data_loss_tracking_session_num - 1] = 
									Int64GetDatum(createMirrorDataLossTrackingSessionNum);

	values[Anum_gp_relation_node_persistent_tid - 1] =
									PointerGetDatum(persistentTid);
	
	values[Anum_gp_relation_node_persistent_serial_num - 1] = 
									Int64GetDatum(persistentSerialNum);
}

