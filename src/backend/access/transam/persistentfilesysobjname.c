/*-------------------------------------------------------------------------
 *
 * persistentfilesysobjname.c
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/transam/persistentfilesysobjname.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "miscadmin.h"
#include "utils/palloc.h"
#include "storage/fd.h"
#include "storage/relfilenode.h"

#include "catalog/catalog.h"
#include "catalog/gp_persistent.h"
#include "access/persistentfilesysobjname.h"
#include "storage/itemptr.h"
#include "utils/hsearch.h"
#include "storage/shmem.h"
#include "access/heapam.h"
#include "access/transam.h"
#include "catalog/pg_tablespace.h"
#include "utils/guc.h"


/*
 * This module is for generic relation file create and drop.
 *
 * For create, it makes the file-system create of an empty file fully transactional so
 * the relation file will be deleted even on system crash.  The relation file could be a heap,
 * index, or append-only (row- or column-store).
 */

// -----------------------------------------------------------------------------
// Helper
// -----------------------------------------------------------------------------

char *PersistentFileSysObjName_ObjectName(
	const PersistentFileSysObjName		*name)
{
	char path[MAXPGPATH + 30];

	/*
	 * We don't use relpath or GetDatabasePath since they include the filespace directory.
	 *
	 * We just want the shorter version for display purposes.
	 */
	switch (name->type)
	{
	case PersistentFsObjType_RelationFile:	
		if (name->variant.rel.relFileNode.spcNode == GLOBALTABLESPACE_OID)
		{
			/* Shared system relations live in {datadir}/global */
			sprintf(path, "global/%u (segment file #%d)",
					name->variant.rel.relFileNode.relNode,
					name->variant.rel.segmentFileNum);
		}
		else if (name->variant.rel.relFileNode.spcNode == DEFAULTTABLESPACE_OID)
		{
			/* The default tablespace is {datadir}/base */
			sprintf(path, "base/%u/%u (segment file #%d)",
					name->variant.rel.relFileNode.dbNode,
					name->variant.rel.relFileNode.relNode,
					name->variant.rel.segmentFileNum);
		}
		else
		{
			/* All other tablespaces are accessed via filespace locations */
			sprintf(path, "%u/%u/%u (segment file #%d)",
					 name->variant.rel.relFileNode.spcNode,
					 name->variant.rel.relFileNode.dbNode,
					 name->variant.rel.relFileNode.relNode,
					 name->variant.rel.segmentFileNum);
		}
		break;
	case PersistentFsObjType_DatabaseDir:	
		if (name->variant.dbDirNode.tablespace == GLOBALTABLESPACE_OID)
		{
			/* Shared system relations live in {datadir}/global */
			strcpy(path, "global");
		}
		else if (name->variant.dbDirNode.tablespace == DEFAULTTABLESPACE_OID)
		{
			/* The default tablespace is {datadir}/base */
			sprintf(path, "base/%u",
					name->variant.dbDirNode.database);
		}
		else
		{
			/* All other tablespaces are accessed via filespace locations */
			sprintf(path, "%u/%u",
					 name->variant.dbDirNode.tablespace,
					 name->variant.dbDirNode.database);
		}
		break;
	case PersistentFsObjType_TablespaceDir:
		sprintf(path, "%u", name->variant.tablespaceOid);
		break;
	case PersistentFsObjType_FilespaceDir:
		sprintf(path, "%u", name->variant.filespaceOid);
		break;

	default:
		elog(ERROR, "Unexpected persistent file-system object type: %d",
			 name->type);
		return pstrdup("Unknown");
	}

	return pstrdup(path);
}

char *PersistentFileSysObjName_TypeName(
	const PersistentFsObjType		type)
{
	switch (type)
	{
	case PersistentFsObjType_RelationFile: 		return "Relation File";
	case PersistentFsObjType_DatabaseDir: 		return "Database Directory";
	case PersistentFsObjType_TablespaceDir: 	return "Tablespace Directory";
	case PersistentFsObjType_FilespaceDir: 		return "Filespace Directory";
	default:
		return "Unknown";
	}
}

char *PersistentFileSysObjName_TypeAndObjectName(
	const PersistentFileSysObjName		*name)
{
	char *typeName;
	char *objectName;

	char resultLen;
	char *result;

	typeName = PersistentFileSysObjName_TypeName(name->type);
	objectName = PersistentFileSysObjName_ObjectName(name);

	resultLen = strlen(typeName) + 4 + strlen(objectName) + 1;
	result = (char*)palloc(resultLen);

	snprintf(result, resultLen, "%s: '%s'", typeName, objectName);

	pfree(objectName);

	return result;
	
}


int PersistentFileSysObjName_Compare(
	const PersistentFileSysObjName		*name1,
	const PersistentFileSysObjName		*name2)
{
	int compareLen = 0;
	int cmp;

	if (name1->type == name2->type)
	{		
		switch (name1->type)
		{
		case PersistentFsObjType_RelationFile:	
//			compareLen = offsetof(PersistentFileSysObjName,variant.rel.segmentFileNum) + sizeof(int32);
			compareLen = sizeof(RelFileNode);
			break;

		case PersistentFsObjType_DatabaseDir:	
			compareLen = sizeof(DbDirNode);
			break;
		case PersistentFsObjType_TablespaceDir: 
			compareLen = sizeof(Oid);
			break;
		case PersistentFsObjType_FilespaceDir:	
			compareLen = sizeof(Oid);
			break;
		default:
			elog(ERROR, "Unexpected persistent file-system object type: %d",
				 name1->type);
			return 0;
		}

		cmp = memcmp(
					&name1->variant, 
					&name2->variant,
					compareLen);
		if (cmp == 0)
		{
			/*
			 * Handle segmentFileNum for 'Relation File's.
			 */
			if (name1->type != PersistentFsObjType_RelationFile)
				return 0;

			if (name1->variant.rel.segmentFileNum == name2->variant.rel.segmentFileNum)
				return 0;
			else if (name1->variant.rel.segmentFileNum > name2->variant.rel.segmentFileNum)
				return 1;
			else
				return -1;
		}
		else if (cmp > 0)
			return 1;
		else
			return -1;
	}
	else if (name1->type > name2->type)
		return 1;
	else
		return -1;
}

char *PersistentFileSysObjState_Name(
	PersistentFileSysState state)
{
	switch (state)
	{
	case PersistentFileSysState_Free: 			return "Free";
	case PersistentFileSysState_CreatePending: 	return "Create Pending";
	case PersistentFileSysState_Created: 		return "Created";
	case PersistentFileSysState_DropPending: 	return "Drop Pending";
	case PersistentFileSysState_AbortingCreate: return "Aborting Create";

	case PersistentFileSysState_JustInTimeCreatePending: return "Just-In-Time Create Pending";

	case PersistentFileSysState_BulkLoadCreatePending: 	return "Bulk Load Create Pending";

	default:
		return "Unknown";
	}
}

char *MirroredObjectExistenceState_Name(
		MirroredObjectExistenceState mirrorExistenceState)
{
	switch (mirrorExistenceState)
	{
	case MirroredObjectExistenceState_None: 					return "None";
	case MirroredObjectExistenceState_NotMirrored: 				return "Not Mirrored";
	case MirroredObjectExistenceState_MirrorCreatePending: 		return "Mirror Create Pending";
	case MirroredObjectExistenceState_MirrorCreated: 			return "Mirror Created";
	case MirroredObjectExistenceState_MirrorDownBeforeCreate: 	return "Mirror Down Before Create";
	case MirroredObjectExistenceState_MirrorDownDuringCreate: 	return "Mirror Down During Create";
	case MirroredObjectExistenceState_MirrorDropPending:		return "Mirror Drop Pending";
	case MirroredObjectExistenceState_OnlyMirrorDropRemains:	return "Only Mirror Drop Remains";

	default:
		return "Unknown";
	}
}	

char *PersistentFileSysRelStorageMgr_Name(
	PersistentFileSysRelStorageMgr relStorageMgr)
{
	switch (relStorageMgr)
	{
	case PersistentFileSysRelStorageMgr_None: 		return "None";
	case PersistentFileSysRelStorageMgr_BufferPool:	return "Buffer Pool";
	case PersistentFileSysRelStorageMgr_AppendOnly:	return "Append-Only";
		
	default:
		return "Unknown";
	}
}

char *MirroredRelDataSynchronizationState_Name(
	MirroredRelDataSynchronizationState relDataSynchronizationState)
{
	switch (relDataSynchronizationState)
	{
	case MirroredRelDataSynchronizationState_None:						return "None";
	case MirroredRelDataSynchronizationState_DataSynchronized:			return "Data Synchronized";
	case MirroredRelDataSynchronizationState_FullCopy:					return "Full Copy Needed";
	case MirroredRelDataSynchronizationState_BufferPoolPageIncremental:	return "Buffer Pool Page Incremental";
	case MirroredRelDataSynchronizationState_BufferPoolScanIncremental:	return "Buffer Pool Scan Incremental";
	case MirroredRelDataSynchronizationState_AppendOnlyCatchup:			return "Append-Only Catch-Up";

	default:
		return "Unknown";
	}
}
