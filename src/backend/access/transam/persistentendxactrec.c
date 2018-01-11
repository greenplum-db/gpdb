/*-------------------------------------------------------------------------
 *
 * persistentendxactrec.c
 *	  Make and read persistent information in commit, abort, and prepared records.
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/transam/persistentendxactrec.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"
#include "access/persistentendxactrec.h"
#include "storage/smgr.h"
#include "storage/smgr_ao.h"
#include "utils/guc.h"
#include "cdb/cdbpersistentstore.h"
#include "cdb/cdbpersistentfilesysobj.h"

char *EndXactRecKind_Name(
	EndXactRecKind endXactRecKind)
{
	switch (endXactRecKind)
	{
	case EndXactRecKind_Commit:		return "Commit";
	case EndXactRecKind_Abort:		return "Abort";
	case EndXactRecKind_Prepare:	return "Prepare";
		
	default:
		return "Unknown";
	}
}

/*
 * Need to comment on what's expected behavior for PREPARE or is this never called for the case
 */
bool EndXactRecKind_NeedsAction(
	EndXactRecKind 					endXactRecKind,

	PersistentEndXactFileSysAction 	action)
{
	switch (action)
	{
	case PersistentEndXactFileSysAction_Create:
		return true;		// Go to 'Created' state on commit, or 'Aborting Create' state and perform DROP on abort.

	case PersistentEndXactFileSysAction_Drop:
		return (endXactRecKind != EndXactRecKind_Abort);
							// Peform DROP on commit.

	case PersistentEndXactFileSysAction_AbortingCreateNeeded:
		return true;		// Clean-up required on commit or abort.
		
	default:
		elog(ERROR, "Unexpected persistent end transaction file-system action: %d",
			 action);
		return false;	// Not reached.
	}
}

char *PersistentEndXactFileSysAction_Name(
	PersistentEndXactFileSysAction action)
{
	switch (action)
	{
	case PersistentEndXactFileSysAction_Create:					return "CREATE";
	case PersistentEndXactFileSysAction_Drop:					return "DROP";
	case PersistentEndXactFileSysAction_AbortingCreateNeeded:	return "Aborting Create Needed";
		
	default:
		return "Unknown";
	}
}

/*
 * Structure containing an object.
 */
typedef struct PersistentEndXactRecObjHdr
{
	int32 	len;
	int16   objKind;
	
	union
	{
		uint8	data[0];
		uint64  align;
	} p;
} PersistentEndXactRecObjHdr;

void PersistentEndXactRec_Print(
	char 							*procName,

	PersistentEndXactRecObjects		*objects)
{
	int i;

	elog(Persistent_DebugPrintLevel(), 
		 "%s: file-system action count %d, Append-Only mirror resync EOFs count %d",
		 procName,
		 objects->typed.fileSysActionInfosCount,
		 objects->typed.appendOnlyMirrorResyncEofsCount);

	for (i = 0; i < objects->typed.fileSysActionInfosCount; i++)
	{
		PersistentEndXactFileSysActionInfo	*fileSysActionInfos =
										&objects->typed.fileSysActionInfos[i];

		elog(Persistent_DebugPrintLevel(), 
			 "%s: [%d] action '%s' %s, relation storage manager '%s', persistent serial num " INT64_FORMAT ", TID %s",
			 procName,
			 i,
			 PersistentEndXactFileSysAction_Name(fileSysActionInfos->action),
			 PersistentFileSysObjName_TypeAndObjectName(&fileSysActionInfos->fsObjName),
			 PersistentFileSysRelStorageMgr_Name(fileSysActionInfos->relStorageMgr),
			 fileSysActionInfos->persistentSerialNum,
			 ItemPointerToString(&fileSysActionInfos->persistentTid));
		
	}

	for (i = 0; i < objects->typed.appendOnlyMirrorResyncEofsCount; i++)
	{
		PersistentEndXactAppendOnlyMirrorResyncEofs *eofs = 
					&objects->typed.appendOnlyMirrorResyncEofs[i];

		elog(Persistent_DebugPrintLevel(), 
			 "%s: [%d] Append-Only Mirror Resync EOFs %u/%u/%u, segment file #%d -- persistent serial num " INT64_FORMAT ", TID %s, mirror loss EOF " INT64_FORMAT ", mirror new EOF " INT64_FORMAT,
			 procName,
			 i,
			 eofs->relFileNode.spcNode,
			 eofs->relFileNode.dbNode,
			 eofs->relFileNode.relNode,
			 eofs->segmentFileNum,
			 eofs->persistentSerialNum,
			 ItemPointerToString(&eofs->persistentTid),
			 eofs->mirrorLossEof,
			 eofs->mirrorNewEof);
	}
}

static int PersistentEndXactRec_CountForObject(
	PersistentEndXactObjKind objKind,

	int32 len)
{
	int count = 0;
	
	switch (objKind)
	{
	case PersistentEndXactObjKind_FileSysAction:
		count = len / sizeof(PersistentEndXactFileSysActionInfo);
		break;
		
	case PersistentEndXactObjKind_AppendOnlyMirrorResyncEofs:
		count = len / sizeof(PersistentEndXactAppendOnlyMirrorResyncEofs);
		break;
		
	default:
		elog(ERROR, "Unexpected persistent transaction object kind: %d",
			 objKind);
	}

	return count;
}

static void PersistentEndXactRec_SetObjectInfo(
	PersistentEndXactRecObjects	*objects,
	
	PersistentEndXactObjKind 	objKind,

	void *data,

	int count,

	int32 len)
{
	PersistentEndXactUntyped *untyped = &objects->untyped[objKind-1];

	untyped->data = data;
	untyped->count = count;
	untyped->len = len;
}

static void PersistentEndXactRec_GetObjectInfo(
	PersistentEndXactRecObjects	*objects,
	
	PersistentEndXactObjKind 	objKind,

	uint8 **data,

	int *count,

	int32 *len)
{
	PersistentEndXactUntyped *untyped = &objects->untyped[objKind-1];

	*data = untyped->data;
	*count = untyped->count;
	*len = untyped->len;
}

void PersistentEndXactRec_Init(
	PersistentEndXactRecObjects	*objects)
{
	MemSet(objects, 0, sizeof(PersistentEndXactRecObjects));
}

static void
PersistentEndXactRec_VerifyFileSysActionInfos(
	EndXactRecKind						endXactRecKind,

	PersistentEndXactFileSysActionInfo	*fileSysActionInfos,

	int 								count)
{
	int i;

	if (InRecovery || Persistent_BeforePersistenceWork())
		return;

	for (i = 0; i < count; i++)
	{
		if (!PersistentEndXactFileSysAction_IsValid(fileSysActionInfos[i].action))
			elog(ERROR, "Persistent file-system action is invalid (%d) (index %d, transaction kind '%s')",
				 fileSysActionInfos[i].action,
				 i,
				 EndXactRecKind_Name(endXactRecKind));
			
		if (!PersistentFsObjType_IsValid(fileSysActionInfos[i].fsObjName.type))
			elog(ERROR, "Persistent file-system object type is invalid (%d) (index %d, transaction kind '%s')",
				 fileSysActionInfos[i].fsObjName.type,
				 i,
				 EndXactRecKind_Name(endXactRecKind));
			
		if (PersistentStore_IsZeroTid(&fileSysActionInfos[i].persistentTid))
			elog(ERROR, "TID for persistent '%s' tuple is invalid (0,0) (index %d, transaction kind '%s')",
				 PersistentFileSysObjName_TypeAndObjectName(&fileSysActionInfos[i].fsObjName),
				 i,
				 EndXactRecKind_Name(endXactRecKind));

		if (fileSysActionInfos[i].persistentSerialNum == 0)
			elog(ERROR, "Persistent '%s' serial number is invalid (0) (index %d, transaction kind '%s')",
				 PersistentFileSysObjName_TypeAndObjectName(&fileSysActionInfos[i].fsObjName),
				 i,
				 EndXactRecKind_Name(endXactRecKind));

		if (fileSysActionInfos[i].fsObjName.type == PersistentFsObjType_RelationFile &&
			!PersistentFileSysRelStorageMgr_IsValid(fileSysActionInfos[i].relStorageMgr))
			elog(ERROR, "Persistent '%s' relation storage manager has invalid value (%d) (index %d, transaction kind '%s')",
				 PersistentFileSysObjName_TypeAndObjectName(&fileSysActionInfos[i].fsObjName),
				 fileSysActionInfos[i].relStorageMgr,
				 i,
				 EndXactRecKind_Name(endXactRecKind));
	}
}

void PersistentEndXactRec_AddFileSysActionInfos(
	PersistentEndXactRecObjects			*objects,

	EndXactRecKind						endXactRecKind,

	PersistentEndXactFileSysActionInfo	*fileSysActionInfos,

	int									count)
{
	objects->typed.fileSysActionInfos = fileSysActionInfos;
	objects->typed.fileSysActionInfosCount = count;
	objects->typed.fileSysActionInfosLen = count * sizeof(PersistentEndXactFileSysActionInfo);

	PersistentEndXactRec_VerifyFileSysActionInfos(
										endXactRecKind,
										fileSysActionInfos,
										count);
}

int32 PersistentEndXactRec_FetchObjectsFromSmgr(
	PersistentEndXactRecObjects	*objects,

	EndXactRecKind				endXactRecKind,

	int16						*objectCount)
{
	int32 serializeLen;

	int objKind;
	void *data;
	int32 count;
	int32 len;

	MemSet(objects, 0, sizeof(PersistentEndXactRecObjects));
	*objectCount = 0;
	serializeLen = 0;

	for (objKind = 1; objKind < MaxPersistentEndXactObjKind; objKind++)
	{
		data = NULL;
		count = 0;
		len = 0;
		switch (objKind)
		{
		case PersistentEndXactObjKind_FileSysAction:
			count = smgrGetPendingFileSysWork(
									endXactRecKind,
									(PersistentEndXactFileSysActionInfo**)&data);
			len = count * sizeof(PersistentEndXactFileSysActionInfo);

			PersistentEndXactRec_VerifyFileSysActionInfos(
										endXactRecKind,
										(PersistentEndXactFileSysActionInfo*)data,
										count);
			break;
			
		case PersistentEndXactObjKind_AppendOnlyMirrorResyncEofs:
			count = smgrGetAppendOnlyMirrorResyncEofs(
									endXactRecKind,
									(PersistentEndXactAppendOnlyMirrorResyncEofs**)&data);
			len = count * sizeof(PersistentEndXactAppendOnlyMirrorResyncEofs);
			break;

		default:
			elog(ERROR, "Unexpected persistent transaction object kind: %d",
				 objKind);
		}

		if (len > 0)
		{
			(*objectCount)++;

			serializeLen += offsetof(PersistentEndXactRecObjHdr, p.data) + len;

			PersistentEndXactRec_SetObjectInfo(
										objects, objKind, data, count, len);
		}
	}

	return serializeLen;
}

bool PersistentEndXactRec_WillHaveObjectsFromSmgr(
	EndXactRecKind				endXactRecKind)
{
	int objKind;

	for (objKind = 1; objKind < MaxPersistentEndXactObjKind; objKind++)
	{
		switch (objKind)
		{
		case PersistentEndXactObjKind_FileSysAction:
			if (smgrIsPendingFileSysWork(endXactRecKind))
			{
				return true;
			}
			break;
			
		case PersistentEndXactObjKind_AppendOnlyMirrorResyncEofs:
			if (smgrIsAppendOnlyMirrorResyncEofs(endXactRecKind))
			{
				return true;
			}
			break;

		default:
			elog(ERROR, "Unexpected persistent transaction object kind: %d",
				 objKind);
		}
	}

	return false;
}

int16 PersistentEndXactRec_ObjectCount(
	PersistentEndXactRecObjects	*objects,
	
	EndXactRecKind				endXactRecKind)
{
	int16 objectCount;

	int objKind;
	uint8 *data;
	int count;
	int32 len;

	objectCount = 0;

	for (objKind = 1; objKind < MaxPersistentEndXactObjKind; objKind++)
	{
		PersistentEndXactRec_GetObjectInfo(
									objects, objKind, &data, &count, &len);
		if (len > 0)
		{
			objectCount++;
		}
	}

	return objectCount;
}


int32 PersistentEndXactRec_SerializeLen(
	PersistentEndXactRecObjects	*objects,
	
	EndXactRecKind				endXactRecKind)
{
	int32 fillLen;

	int objKind;
	uint8 *data;
	int count;
	int32 len;

	fillLen = 0;

	for (objKind = 1; objKind < MaxPersistentEndXactObjKind; objKind++)
	{
		PersistentEndXactRec_GetObjectInfo(
									objects, objKind, &data, &count, &len);
		if (len > 0)
		{
			fillLen += offsetof(PersistentEndXactRecObjHdr, p.data) + len;
		}
	}

	return fillLen;
}

static void PersistentEndXactRec_AddSection(
	uint8 						*buffer,

	int32						bufferLen,

	int32						*currentLen,

	PersistentEndXactObjKind	objKind,

	uint8						*data,

	int32						len)
{
	uint8 *next;
	PersistentEndXactRecObjHdr *header;
	const int headerLen = offsetof(PersistentEndXactRecObjHdr, p.data);

	Assert(len > 0);
	Assert(*currentLen + headerLen + len <= bufferLen);

	next = &buffer[*currentLen];
	
	header = (PersistentEndXactRecObjHdr*)next;
	MemSet(header, 0, headerLen);
	header->len = len;
	header->objKind = objKind;
	
	memcpy(header->p.data, data, len);

	(*currentLen) += headerLen + len;
}


void PersistentEndXactRec_Serialize(
	PersistentEndXactRecObjects	*objects,

	EndXactRecKind				endXactRecKind,

	int16						*numOfObjects,

	uint8 						*buffer,

	int32						bufferLen)

{
	int objKind;
	int32 currentLen;
	uint8 *data;
	int count;
	int32 len;

	*numOfObjects = 0;

	currentLen = 0;
	for (objKind = 1; objKind < MaxPersistentEndXactObjKind; objKind++)
	{
		PersistentEndXactRec_GetObjectInfo(
									objects, objKind, &data, &count, &len);
		if (len > 0)
		{
			PersistentEndXactRec_AddSection(
										buffer,
										bufferLen,
										&currentLen,
										objKind,
										data,
										len);
			(*numOfObjects)++;
		}
	}

	if (currentLen != bufferLen)
	{
		elog(ERROR, "Persistent end transaction serialize length mismatch (actual %d, supplied %d)",
		     currentLen, bufferLen);
	}
}

int32 PersistentEndXactRec_DeserializeLen(
	uint8						*buffer,

	int16						numOfObjects)
{
	int deserializeLen;

	int n;
	PersistentEndXactRecObjHdr *header;
	int32 len;

	deserializeLen = 0;
	header = (PersistentEndXactRecObjHdr*)buffer;
	for (n = 0; n < numOfObjects; n++)
	{
		len = header->len;

		deserializeLen += offsetof(PersistentEndXactRecObjHdr, p.data) + len;
		header = (PersistentEndXactRecObjHdr*)&buffer[deserializeLen];
	}

	return deserializeLen;
}

void PersistentEndXactRec_Deserialize(
	uint8						*buffer,

	int16						numOfObjects,

	PersistentEndXactRecObjects	*objects,

	uint8						**nextData)
{
	int32 currentLen;
	int n;
	int objKind;
	PersistentEndXactRecObjHdr *header;
	int32 len;
	uint8 *data;
	int count;

	MemSet(objects, 0, sizeof(PersistentEndXactRecObjects));

	currentLen = 0;
	header = (PersistentEndXactRecObjHdr*)buffer;
	for (n = 0; n < numOfObjects; n++)
	{

		len = header->len;
		objKind = header->objKind;
		data = header->p.data;

		count = PersistentEndXactRec_CountForObject(objKind, len);

		PersistentEndXactRec_SetObjectInfo(
									objects, objKind, data, count, len);

		currentLen += offsetof(PersistentEndXactRecObjHdr, p.data) + len;
		header = (PersistentEndXactRecObjHdr*)&buffer[currentLen];
	}

	*nextData = (uint8*)header;
}
