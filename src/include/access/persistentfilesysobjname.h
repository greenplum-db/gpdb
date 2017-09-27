/*-------------------------------------------------------------------------
 *
 * persistentfilesysobjname.h
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/access/persistentfilesysobjname.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PERSISTENTFILESYSOBJNAME_H
#define PERSISTENTFILESYSOBJNAME_H

#include "utils/palloc.h"
#include "storage/fd.h"
#include "storage/relfilenode.h"
#include "storage/dbdirnode.h"
#include "storage/tablespacedirnode.h"

typedef enum PersistentFsObjType
{
	PersistentFsObjType_First = 0, //Must start at 0 for 0 - based indexing.
	PersistentFsObjType_RelationFile = PersistentFsObjType_First,
	PersistentFsObjType_DatabaseDir = 1,
	PersistentFsObjType_TablespaceDir = 2,
	PersistentFsObjType_FilespaceDir = 3,
	PersistentFsObjType_Last = PersistentFsObjType_FilespaceDir,
	CountPersistentFsObjType = PersistentFsObjType_Last - PersistentFsObjType_First + 1
} PersistentFsObjType;

inline
	static bool PersistentFsObjType_IsValid(
											PersistentFsObjType fsObjType)
	{
		return (fsObjType >= PersistentFsObjType_First &&
				fsObjType <= PersistentFsObjType_Last);
	}

typedef union PersistentFileSysObjNameVariant
{
	struct rel
	{
		RelFileNode relFileNode;

		int32		segmentFileNum;
	}			rel;

	DbDirNode	dbDirNode;

	Oid			tablespaceOid;

	Oid			filespaceOid;

} PersistentFileSysObjNameVariant;

typedef struct PersistentFileSysObjName
{
	PersistentFsObjType type;

	PersistentFileSysObjNameVariant variant;
} PersistentFileSysObjName;

inline static void
PersistentFileSysObjName_SetRelationFile(
										 PersistentFileSysObjName *fsObjName,
										 RelFileNode *relFileNode,
										 int32 segmentFileNum)
{
	MemSet(fsObjName, 0, sizeof(PersistentFileSysObjName));
	fsObjName->type = PersistentFsObjType_RelationFile;
	memcpy(&(fsObjName->variant.rel.relFileNode), relFileNode, sizeof(RelFileNode));
	fsObjName->variant.rel.segmentFileNum = segmentFileNum;
}

inline static RelFileNode
PersistentFileSysObjName_GetRelFileNode(
										PersistentFileSysObjName *fsObjName)
{
	Assert(fsObjName->type == PersistentFsObjType_RelationFile);

	return fsObjName->variant.rel.relFileNode;
}

inline static RelFileNode *
PersistentFileSysObjName_GetRelFileNodePtr(
										   PersistentFileSysObjName *fsObjName)
{
	Assert(fsObjName->type == PersistentFsObjType_RelationFile);

	return &fsObjName->variant.rel.relFileNode;
}

inline static int32
PersistentFileSysObjName_GetSegmentFileNum(
										   PersistentFileSysObjName *fsObjName)
{
	Assert(fsObjName->type == PersistentFsObjType_RelationFile);

	return fsObjName->variant.rel.segmentFileNum;
}

inline static void
PersistentFileSysObjName_SetDatabaseDir(
										PersistentFileSysObjName *fsObjName,
										Oid tablespaceOid,
										Oid databaseOid)
{
	MemSet(fsObjName, 0, sizeof(PersistentFileSysObjName));
	fsObjName->type = PersistentFsObjType_DatabaseDir;
	fsObjName->variant.dbDirNode.tablespace = tablespaceOid;
	fsObjName->variant.dbDirNode.database = databaseOid;
}

inline static DbDirNode *
PersistentFileSysObjName_GetDbDirNodePtr(
										 PersistentFileSysObjName *fsObjName)
{
	Assert(fsObjName->type == PersistentFsObjType_DatabaseDir);

	return &fsObjName->variant.dbDirNode;
}

inline static void
PersistentFileSysObjName_SetTablespaceDir(
										  PersistentFileSysObjName *fsObjName,
										  Oid tablespaceOid)
{
	MemSet(fsObjName, 0, sizeof(PersistentFileSysObjName));
	fsObjName->type = PersistentFsObjType_TablespaceDir;
	fsObjName->variant.tablespaceOid = tablespaceOid;
}

inline static Oid
PersistentFileSysObjName_GetTablespaceDir(
										  PersistentFileSysObjName *fsObjName)
{
	Assert(fsObjName->type == PersistentFsObjType_TablespaceDir);

	return fsObjName->variant.tablespaceOid;
}

inline static void
PersistentFileSysObjName_SetFilespaceDir(
										 PersistentFileSysObjName *fsObjName,
										 Oid filespaceOid)
{
	MemSet(fsObjName, 0, sizeof(PersistentFileSysObjName));
	fsObjName->type = PersistentFsObjType_FilespaceDir;
	fsObjName->variant.filespaceOid = filespaceOid;
}

inline static Oid
PersistentFileSysObjName_GetFilespaceDir(
										 PersistentFileSysObjName *fsObjName)
{
	Assert(fsObjName->type == PersistentFsObjType_FilespaceDir);

	return fsObjName->variant.filespaceOid;
}

/*
 * The file kinds of a persistent file-system object.
 */
typedef enum PersistentFileSysRelStorageMgr
{
	PersistentFileSysRelStorageMgr_None = 0,
	PersistentFileSysRelStorageMgr_BufferPool = 1,
	PersistentFileSysRelStorageMgr_AppendOnly = 2,
	MaxPersistentFileSysRelStorageMgr	/* must always be last */
} PersistentFileSysRelStorageMgr;

inline static bool
PersistentFileSysRelStorageMgr_IsValid(
									   PersistentFileSysRelStorageMgr relStorageMgr)
{
	return (relStorageMgr == PersistentFileSysRelStorageMgr_BufferPool ||
			relStorageMgr == PersistentFileSysRelStorageMgr_AppendOnly);
}


/*
 * The Buffer Pool relation kinds of a persistent file-system object.
 */
typedef enum PersistentFileSysRelBufpoolKind
{
	PersistentFileSysRelBufpoolKind_None = 0,
	PersistentFileSysRelBufpoolKind_Heap = 1,
	PersistentFileSysRelBufpoolKind_UnknownRelStorage = 2,
	PersistentFileSysRelBufpoolKind_AppendOnlySeginfo = 3,
	PersistentFileSysRelBufpoolKind_AppendOnlyBlockDirectory = 4,
	PersistentFileSysRelBufpoolKind_AppendOnlyVisibilityMap = 12,
	PersistentFileSysRelBufpoolKind_Btree = 5,
	PersistentFileSysRelBufpoolKind_BitMap = 6,
	PersistentFileSysRelBufpoolKind_UnknownIndex = 7,
	PersistentFileSysRelBufpoolKind_Sequence = 8,
	PersistentFileSysRelBufpoolKind_Toast = 9,
	PersistentFileSysRelBufpoolKind_UncatalogedHeap = 10,
	PersistentFileSysRelBufpoolKind_UnknownRelKind = 11,
	MaxPersistentFileSysRelBufpoolKind = 13 /* must always be last */
} PersistentFileSysRelBufpoolKind;

/*
 * The states of a persistent file-system object.
 */
typedef enum PersistentFileSysState
{
	PersistentFileSysState_Free = 0,
	PersistentFileSysState_CreatePending = 1,
	PersistentFileSysState_Created = 2,
	PersistentFileSysState_DropPending = 3,
	PersistentFileSysState_AbortingCreate = 4,
	PersistentFileSysState_JustInTimeCreatePending = 5,
	PersistentFileSysState_BulkLoadCreatePending,
	MaxPersistentFileSysState	/* must always be last */
} PersistentFileSysState;

/*
 * The primary's view of the state of a mirrored persistent file-system
 * object.
 */
typedef enum MirroredObjectExistenceState
{
	MirroredObjectExistenceState_None = 0,
	MirroredObjectExistenceState_NotMirrored = 1,
	MirroredObjectExistenceState_MirrorCreatePending = 2,
	MirroredObjectExistenceState_MirrorCreated = 3,
	MirroredObjectExistenceState_MirrorDownBeforeCreate = 4,
	MirroredObjectExistenceState_MirrorDownDuringCreate = 5,
	MirroredObjectExistenceState_MirrorDropPending = 6,
	MirroredObjectExistenceState_OnlyMirrorDropRemains = 7,
	MaxMirroredObjectExistenceState /* must always be last */
} MirroredObjectExistenceState;

inline static bool
MirroredObjectExistenceState_IsResynchCreated(
											  MirroredObjectExistenceState mirrorExistenceState)
{
	return (mirrorExistenceState == MirroredObjectExistenceState_MirrorCreatePending ||
			mirrorExistenceState == MirroredObjectExistenceState_MirrorCreated ||
			mirrorExistenceState == MirroredObjectExistenceState_MirrorDownBeforeCreate ||
			mirrorExistenceState == MirroredObjectExistenceState_MirrorDownDuringCreate);
}


/*
 * The primary's view of the state of a mirrored persistent relation data
 * synchronization state.
 */
typedef enum MirroredRelDataSynchronizationState
{
	MirroredRelDataSynchronizationState_None = 0,
	MirroredRelDataSynchronizationState_DataSynchronized = 1,
	MirroredRelDataSynchronizationState_FullCopy = 2,
	MirroredRelDataSynchronizationState_BufferPoolPageIncremental = 3,
	MirroredRelDataSynchronizationState_BufferPoolScanIncremental = 4,
	MirroredRelDataSynchronizationState_AppendOnlyCatchup = 5,
	MaxMirroredRelDataSynchronizationState	/* must always be last */
} MirroredRelDataSynchronizationState;


/*
 * This module is for generic relation file create and drop.
 *
 * For create, it makes the file-system create of an empty file fully transactional so
 * the relation file will be deleted even on system crash.  The relation file could be a heap,
 * index, or append-only (row- or column-store).
 */

/* ----------------------------------------------------------------------------- */
/*  Helper */
/* ----------------------------------------------------------------------------- */

extern char *PersistentFileSysObjName_ObjectName(
									const PersistentFileSysObjName *name);

extern char *PersistentFileSysObjName_TypeName(
								  PersistentFsObjType type);

extern char *PersistentFileSysObjName_TypeAndObjectName(
										   const PersistentFileSysObjName *name);

extern int PersistentFileSysObjName_Compare(
								 const PersistentFileSysObjName *name1,
								 const PersistentFileSysObjName *name2);

extern char *PersistentFileSysObjState_Name(
							   PersistentFileSysState state);

extern char *MirroredObjectExistenceState_Name(
								  MirroredObjectExistenceState mirrorExistenceState);

extern char *PersistentFileSysRelStorageMgr_Name(
									PersistentFileSysRelStorageMgr relStorageMgr);

extern char *MirroredRelDataSynchronizationState_Name(
										 MirroredRelDataSynchronizationState relDataSynchronizationState);

#endif							/* PERSISTENTFILESYSOBJNAME_H */
