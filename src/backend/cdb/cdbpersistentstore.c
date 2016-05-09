/*-------------------------------------------------------------------------
 *
 * cdbpersistentstore.c
 *
 * Copyright (c) 2009-2010, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "utils/palloc.h"

#include "cdb/cdbpersistentstore.h"
#include "cdb/cdbpersistentcheck.h"

#include "storage/itemptr.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/transam.h"
#include "utils/guc.h"
#include "storage/smgr.h"
#include "storage/ipc.h"
#include "cdb/cdbglobalsequence.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "cdb/cdbpersistentrecovery.h"

void PersistentStore_InitShared(
	PersistentStoreSharedData 	*storeSharedData)
{
	MemSet(storeSharedData, 0, sizeof(PersistentStoreSharedData));
	strcpy(storeSharedData->eyecatcher, PersistentStoreSharedDataEyecatcher);
	storeSharedData->needToScanIntoSharedMemory = true;	
}

void PersistentStore_Init(
	PersistentStoreData 		*storeData,
	char						*tableName,
	GpGlobalSequence			gpGlobalSequence,
	PersistentStoreOpenRel		openRel,
	PersistentStoreCloseRel		closeRel,
	PersistentStoreScanTupleCallback	scanTupleCallback,
	PersistentStorePrintTupleCallback	printTupleCallback,
	PersistentStoreScanKeyInitCallback  scanKeyInitCallback,
	PersistentStoreAllowDuplicateCallback allowDuplicateCallback,
	int 						numAttributes,
	int 						attNumPersistentSerialNum,
	int 						attNumPreviousFreeTid)
{
	MemSet(storeData, 0, sizeof(PersistentStoreData));

	storeData->tableName = tableName;
	storeData->gpGlobalSequence = gpGlobalSequence;
	storeData->openRel = openRel;
	storeData->closeRel = closeRel;
	storeData->scanTupleCallback = scanTupleCallback;
	storeData->printTupleCallback = printTupleCallback;
	storeData->scanKeyInitCallback = scanKeyInitCallback;
	storeData->allowDuplicateCallback = allowDuplicateCallback;
	storeData->numAttributes = numAttributes;
	storeData->attNumPersistentSerialNum = attNumPersistentSerialNum;
	storeData->attNumPreviousFreeTid = attNumPreviousFreeTid;
}

void PersistentStore_ResetFreeList(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData)
{
	int64 persistentSerialNum;

	persistentSerialNum = 
				GlobalSequence_Current(
						storeData->gpGlobalSequence);

	storeSharedData->maxInUseSerialNum = persistentSerialNum;
	storeSharedData->inUseCount = 0;
	storeSharedData->maxFreeOrderNum = 0;
	MemSet(&storeSharedData->freeTid, 0, sizeof(ItemPointerData));
	MemSet(&storeSharedData->maxTid, 0, sizeof(ItemPointerData));
}

void PersistentStore_DeformTuple(
	PersistentStoreData 	*storeData,
	TupleDesc				tupleDesc,			
						/* tuple descriptor */
    HeapTuple 				tuple,
	Datum					*values)
{
	bool	*nulls;
	int i;

	nulls = (bool*)palloc(storeData->numAttributes * sizeof(bool));
	heap_deform_tuple(tuple, tupleDesc, values, nulls);

	for (i = 1; i <= storeData->numAttributes; i++)
		Assert(!nulls[i - 1]);

	pfree(nulls);
}

static void PersistentStore_ExtractOurTupleData(
	PersistentStoreData 	*storeData,
	Datum					*values,
	int64					*persistentSerialNum,
	ItemPointer				previousFreeTid)
{
	*persistentSerialNum = DatumGetInt64(values[storeData->attNumPersistentSerialNum - 1]);
	*previousFreeTid = *((ItemPointer) DatumGetPointer(values[storeData->attNumPreviousFreeTid - 1]));
}

static void PersistentStore_FormTupleSetOurs(
	PersistentStoreData 	*storeData,
	TupleDesc				tupleDesc,
				/* Tuple descriptor. */
	Datum					*values,
	int64					persistentSerialNum,
	ItemPointerData			*previousFreeTid,
	HeapTuple	*tuple)
				/* The formed tuple. */
{
	int			natts = 0;
	bool	*nulls;

	natts = tupleDesc->natts;
	Assert(natts == storeData->numAttributes);

	/*
	 * In order to keep the tuples the exact same size to enable direct reuse of
	 * free tuples, we do not use NULLs.
	 */
	nulls = (bool*)palloc0(storeData->numAttributes * sizeof(bool));

	values[storeData->attNumPersistentSerialNum - 1] = 
											Int64GetDatum(persistentSerialNum);

	values[storeData->attNumPreviousFreeTid - 1] =
											PointerGetDatum(previousFreeTid);
		
	/*
	 * Form the tuple.
	 */
	*tuple = heap_form_tuple(tupleDesc, values, nulls);
	if (!HeapTupleIsValid(*tuple))
		elog(ERROR, "Failed to build persistent relation node tuple");

}

static HTAB* freeEntryHashTable = NULL;

typedef struct PersistentFreeEntryKey
{
	ItemPointerData	persistentTid;
} PersistentFreeEntryKey;

typedef struct PersistentFreeEntry
{
	PersistentFreeEntryKey key;		
	ItemPointerData previousFreeTid;
	int64			freeOrderNum;
} PersistentFreeEntry;

static void
PersistentStore_FreeEntryHashTableInit(void)
{
	HASHCTL			info;
	int				hash_flags;

	/* Set key and entry sizes. */
	MemSet(&info, 0, sizeof(info));
	info.keysize = sizeof(PersistentFreeEntryKey);
	info.entrysize = sizeof(PersistentFreeEntry);
	info.hash = tag_hash;

	hash_flags = (HASH_ELEM | HASH_FUNCTION);

	freeEntryHashTable = hash_create("Persistent Free Entry", 10, &info, hash_flags);
}

static void PersistentStore_DiagnoseDumpTable(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData)
{

	if (disable_persistent_diagnostic_dump)
	{
		return;
	}

	MIRROREDLOCK_BUFMGR_DECLARE;

	PersistentStoreScan storeScan;
	ItemPointerData			persistentTid;
	int64					persistentSerialNum;
	Datum					*values;
	BlockNumber				lastDisplayedBlockNum;
	bool					displayedOne;
	BlockNumber				currentBlockNum;

	elog(LOG, 
		 "Diagnostic dump of persistent table ('%s'): maximum in-use serial number " INT64_FORMAT ", maximum free order number " INT64_FORMAT ", free TID %s, maximum known TID %s",
		 storeData->tableName,
		 storeSharedData->maxInUseSerialNum, 
		 storeSharedData->maxFreeOrderNum, 
		 ItemPointerToString(&storeSharedData->freeTid),
		 ItemPointerToString2(&storeSharedData->maxTid));

	values = (Datum*)palloc(storeData->numAttributes * sizeof(Datum));

	PersistentStore_BeginScan(
						storeData,
						storeSharedData,
						&storeScan);

	lastDisplayedBlockNum = 0;
	displayedOne = false;
	while (PersistentStore_GetNext(
							&storeScan,
							values,
							&persistentTid,
							&persistentSerialNum))
	{
		/*
		 * Use the BlockIdGetBlockNumber routine because ItemPointerGetBlockNumber 
		 * asserts for valid TID.
		 */
		currentBlockNum = BlockIdGetBlockNumber(&persistentTid.ip_blkid);
		if (!displayedOne || currentBlockNum != lastDisplayedBlockNum)
		{
			Buffer		buffer;
			PageHeader	page;
			XLogRecPtr	lsn;

			/*
			 * Fetch the block and display the LSN.
			 */
			
			// -------- MirroredLock ----------
			MIRROREDLOCK_BUFMGR_LOCK;
			
			buffer = ReadBuffer(
							storeScan.persistentRel,
							currentBlockNum);

			page = (PageHeader) BufferGetPage(buffer);
			lsn = PageGetLSN(page);
			ReleaseBuffer(buffer);

			MIRROREDLOCK_BUFMGR_UNLOCK;
			// -------- MirroredLock ----------

			elog(LOG, "Diagnostic LSN %s of page %u",
				 XLogLocationToString(&lsn),
				 currentBlockNum);

			lastDisplayedBlockNum = currentBlockNum;
			displayedOne = true;
		}

		/*
		 * Display the persistent tuple.
		 */
		(*storeData->printTupleCallback)(
									LOG,
									"DIAGNOSE",
									&persistentTid,
									values);
	}
	
	PersistentStore_EndScan(&storeScan);

	pfree(values);
}

static void PersistentStore_InitScanAddFreeEntry(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData,
	ItemPointer		persistentTid,
	ItemPointer		previousFreeTid,
	int64			freeOrderNum)
{
	PersistentFreeEntryKey key;
	PersistentFreeEntry *entry;
	bool found;

	if (PersistentStore_IsZeroTid(persistentTid))
	{
		PersistentStore_DiagnoseDumpTable(storeData, storeSharedData);
		elog(ERROR, "Expected persistent TID to not be (0,0)");
	}

	if (PersistentStore_IsZeroTid(previousFreeTid))
	{
		elog(ERROR, "Expected previous free TID to not be (0,0)");
	}

	if (freeEntryHashTable == NULL)
		PersistentStore_FreeEntryHashTableInit();

	MemSet(&key, 0, sizeof(key));
	key.persistentTid = *persistentTid;

	entry = 
		(PersistentFreeEntry*) 
						hash_search(freeEntryHashTable,
									(void *) &key,
									HASH_ENTER,
									&found);

	if (found)
	{
		PersistentStore_DiagnoseDumpTable(storeData, storeSharedData);
		elog(ERROR, "Duplicate free persistent TID entry %s",
			 ItemPointerToString(persistentTid));
	}

	entry->previousFreeTid = *previousFreeTid;
	entry->freeOrderNum = freeOrderNum;
}

static void PersistentStore_InitScanVerifyFreeEntries(
	PersistentStoreData 		*storeData,

	PersistentStoreSharedData 	*storeSharedData)
{
	int64 freeOrderNum;
	ItemPointerData freeTid;
	PersistentFreeEntry *entry;
	HASH_SEQ_STATUS iterateStatus;

	freeOrderNum = storeSharedData->maxFreeOrderNum;
	freeTid = storeSharedData->freeTid;

	if (freeOrderNum == 0)
	{
		if (!PersistentStore_IsZeroTid(&freeTid))
		{
			PersistentStore_DiagnoseDumpTable(storeData, storeSharedData);
			elog(ERROR, "Expected free TID to be (0,0) when free order number is 0 in '%s'",
				 storeData->tableName);
		}
	}
	else
	{
		PersistentFreeEntryKey key;
		PersistentFreeEntry *removeEntry;
		
		if (freeEntryHashTable == NULL)
		{
			PersistentStore_DiagnoseDumpTable(storeData, storeSharedData);
			elog(ERROR, "Looking for free order number " INT64_FORMAT " and the free entry hash table is empty for '%s'",
				 freeOrderNum,
				 storeData->tableName);
		}
		
		while (true)
		{
			MemSet(&key, 0, sizeof(key));
			key.persistentTid = freeTid;
			
			entry = 
				(PersistentFreeEntry*) 
								hash_search(freeEntryHashTable,
											(void *) &key,
											HASH_FIND,
											NULL);
			if (entry == NULL)
			{
				PersistentStore_DiagnoseDumpTable(storeData, storeSharedData);
				elog(ERROR, 
					 "Did not find free entry for free TID %s (free order number " INT64_FORMAT ") for '%s'",
					 ItemPointerToString(&freeTid),
					 freeOrderNum,
					 storeData->tableName);
			}

			if (PersistentStore_IsZeroTid(&entry->previousFreeTid))
			{
				PersistentStore_DiagnoseDumpTable(storeData, storeSharedData);
				elog(ERROR, 
					 "Previous free TID not expected to be (0,0) -- persistent Free Entry hashtable corrupted for '%s' "
					 "(expected free order number " INT64_FORMAT ", entry free order number " INT64_FORMAT ")",
				     storeData->tableName,
				     freeOrderNum,
				     entry->freeOrderNum);
			}

			if (freeOrderNum != entry->freeOrderNum)
			{
				PersistentStore_DiagnoseDumpTable(storeData, storeSharedData);
				elog(ERROR, 
					 "Free entry for free TID %s has wrong free order number (expected free order number " INT64_FORMAT ", found free order number " INT64_FORMAT ") for '%s'",
					 ItemPointerToString(&freeTid),
					 freeOrderNum,
					 entry->freeOrderNum,
					 storeData->tableName);
			}
			
			if (Debug_persistent_recovery_print)
				elog(PersistentRecovery_DebugPrintLevel(), 
					 "PersistentStore_InitScanVerifyFreeEntries ('%s'): Free order number " INT64_FORMAT ", free TID %s, previous free TID %s",
					 storeData->tableName,
					 freeOrderNum,
					 ItemPointerToString(&freeTid),
					 ItemPointerToString2(&entry->previousFreeTid));

			freeTid = entry->previousFreeTid;
			Insist(!PersistentStore_IsZeroTid(&freeTid));	// Note the error check above.
			if (freeOrderNum == 1)
			{
				/*
				 * The last free entry uses its own TID in previous_free_tid.
				 */
				if (ItemPointerCompare(
									&entry->key.persistentTid,
									&freeTid) != 0)
				{
					PersistentStore_DiagnoseDumpTable(storeData, storeSharedData);
					elog(ERROR, "Expected previous_free_tid %s to match the persistent TID %s for the last free entry (free order number 1) for '%s'",
						 ItemPointerToString(&freeTid),
						 ItemPointerToString2(&entry->key.persistentTid),
						 storeData->tableName);
						 
				}
			}

			removeEntry = hash_search(
								freeEntryHashTable, 
								(void *) &entry->key, 
								HASH_REMOVE, 
								NULL);
			if (removeEntry == NULL)
			{
				PersistentStore_DiagnoseDumpTable(storeData, storeSharedData);
				elog(ERROR, "Persistent Free Entry hashtable corrupted for '%s'",
				     storeData->tableName);
			}
			entry = NULL;

			freeOrderNum--;

			if (freeOrderNum == 0)
				break;
		
		}
	}

	if (freeEntryHashTable != NULL)
	{
		hash_seq_init(
				&iterateStatus, 
				freeEntryHashTable);

		/*
		 * Verify the hash table has no free entries left.
		 */
		while ((entry = hash_seq_search(&iterateStatus)) != NULL)
		{
			PersistentStore_DiagnoseDumpTable(storeData, storeSharedData);
			elog(ERROR, "Found at least one unaccounted for free entry for '%s'.  Example: free order number " INT64_FORMAT ", free TID %s, previous free TID %s",
				storeData->tableName,
				entry->freeOrderNum,
				ItemPointerToString(&entry->key.persistentTid),
				ItemPointerToString2(&entry->previousFreeTid));
		}

		hash_destroy(freeEntryHashTable);
		freeEntryHashTable = NULL;
	}
	
	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
			 "PersistentStore_InitScanVerifyFreeEntries ('%s'): Successfully verified " INT64_FORMAT " free entries",
			 storeData->tableName,
			 storeSharedData->maxFreeOrderNum);
}

static void PersistentStore_DoInitScan(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData)
{
	PersistentStoreScan 	storeScan;
	ItemPointerData			persistentTid;
	int64					persistentSerialNum;
	ItemPointerData			previousFreeTid;
	Datum					*values;
	int64					globalSequenceNum;
	int64					numFreeEntries = 0;

	values = (Datum*)palloc(storeData->numAttributes * sizeof(Datum));

	MemSet(&storeSharedData->maxTid, 0, sizeof(ItemPointerData));

	PersistentStore_BeginScan(
						storeData,
						storeSharedData,
						&storeScan);

	while (PersistentStore_GetNext(
							&storeScan,
							values,
							&persistentTid,
							&persistentSerialNum))
	{
		/*
		 * We are scanning from low to high TID.
		 */
		Assert(
			PersistentStore_IsZeroTid(&storeSharedData->maxTid)
			||
			ItemPointerCompare(
							&storeSharedData->maxTid,
							&persistentTid) == -1);	// Less-Than.
							
		storeSharedData->maxTid = persistentTid;

		PersistentStore_ExtractOurTupleData(
									storeData,
									values,
									&persistentSerialNum,
									&previousFreeTid);
		
		if (Debug_persistent_recovery_print)
			(*storeData->printTupleCallback)(
										PersistentRecovery_DebugPrintLevel(),
										"SCAN",
										&persistentTid,
										values);
		
		if (!PersistentStore_IsZeroTid(&previousFreeTid))
		{
			/*
			 * Non-zero previousFreeTid implies a free entry.
			 */
			if (storeSharedData->maxFreeOrderNum < persistentSerialNum)
			{
				storeSharedData->maxFreeOrderNum = persistentSerialNum;
				storeSharedData->freeTid = persistentTid;
			}

			if (!gp_persistent_skip_free_list)
			{
				numFreeEntries++;
				PersistentStore_InitScanAddFreeEntry(
												storeData,
												storeSharedData,
												&persistentTid,
												&previousFreeTid,
												/* freeOrderNum */ persistentSerialNum);
			}
		}
		else 
		{
			storeSharedData->inUseCount++;

			if (storeSharedData->maxInUseSerialNum < persistentSerialNum)
			{
				storeSharedData->maxInUseSerialNum = persistentSerialNum;
				storeData->myHighestSerialNum = storeSharedData->maxInUseSerialNum;
			}
		}

		if (storeData->scanTupleCallback != NULL)
			(*storeData->scanTupleCallback)(
										&persistentTid,
										persistentSerialNum,
										values);

	}

	PersistentStore_EndScan(&storeScan);

	pfree(values);

	globalSequenceNum = GlobalSequence_Current(storeData->gpGlobalSequence);

	/*
	 * Note: Originally the below IF STMT was guarded with a InRecovery flag check.
	 * However, this routine should not be called during recovery since the entries are
	 * not consistent...
	 */
	Assert(!InRecovery);
	
	if (globalSequenceNum < storeSharedData->maxInUseSerialNum)
	{
		/*
		 * We seem to have a corruption problem.
		 *
		 * Use the gp_persistent_repair_global_sequence GUC to get the system up.
		 */

		if (gp_persistent_repair_global_sequence)
		{
			elog(LOG, "Need to Repair global sequence number " INT64_FORMAT " so use scanned maximum value " INT64_FORMAT " ('%s')",
				 globalSequenceNum,
				 storeSharedData->maxInUseSerialNum,
				 storeData->tableName);
		}
		else
		{
			elog(ERROR, "Global sequence number " INT64_FORMAT " less than maximum value " INT64_FORMAT " found in scan ('%s')",
				 globalSequenceNum,
				 storeSharedData->maxInUseSerialNum,
				 storeData->tableName);
		}
		
	}
	else
	{
		storeSharedData->maxInUseSerialNum = globalSequenceNum;
	}

	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(),
			 "PersistentStore_DoInitScan ('%s'): maximum in-use serial number " INT64_FORMAT ", maximum free order number " INT64_FORMAT ", free TID %s, maximum known TID %s",
			 storeData->tableName,
			 storeSharedData->maxInUseSerialNum, 
			 storeSharedData->maxFreeOrderNum, 
			 ItemPointerToString(&storeSharedData->freeTid),
			 ItemPointerToString2(&storeSharedData->maxTid));

	if (!gp_persistent_skip_free_list)
	{
		if (numFreeEntries != storeSharedData->maxFreeOrderNum)
		{
			PersistentStore_DiagnoseDumpTable(storeData, storeSharedData);
			elog(ERROR, 
					 "Number of freeTIDs " INT64_FORMAT ", do not match maximum free order number " INT64_FORMAT ", for '%s'",
					 numFreeEntries,
					 storeSharedData->maxFreeOrderNum,
					 storeData->tableName);
		}
		
		PersistentStore_InitScanVerifyFreeEntries(
											storeData,
											storeSharedData);
	}
	else
	{
		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
				 "PersistentStore_DoInitScan ('%s'): Skipping verification because gp_persistent_skip_free_list GUC is ON",
				 storeData->tableName);
	}
}

void PersistentStore_InitScanUnderLock(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData)
{
	if (!storeSharedData->needToScanIntoSharedMemory)
	{
		return;	// Someone else got in first.
	}

	PersistentStore_DoInitScan(
						storeData,
						storeSharedData);

	storeSharedData->needToScanIntoSharedMemory = false;
}

void PersistentStore_Scan(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData,
	PersistentStoreScanTupleCallback	scanTupleCallback)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentStoreScan 	storeScan;
	ItemPointerData			persistentTid;
	int64					persistentSerialNum = 0;
	Datum					*values;

	values = (Datum*)palloc(storeData->numAttributes * sizeof(Datum));

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	PersistentStore_BeginScan(
						storeData,
						storeSharedData,
						&storeScan);

	while (PersistentStore_GetNext(
							&storeScan,
							values,
							&persistentTid,
							&persistentSerialNum))
	{
		bool okToContinue;

		okToContinue = (*scanTupleCallback)(
								&persistentTid,
								persistentSerialNum,
								values);

		if (!okToContinue)
			break;

	}
	
	PersistentStore_EndScan(&storeScan);

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	pfree(values);
}

void PersistentStore_BeginScan(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData	*storeSharedData,
	PersistentStoreScan			*storeScan)
{
	MemSet(storeScan, 0, sizeof(PersistentStoreScan));

	storeScan->storeData = storeData;
	storeScan->storeSharedData = storeSharedData;

	storeScan->persistentRel = (*storeData->openRel)();

	storeScan->scan = heap_beginscan(
							storeScan->persistentRel, 
							SnapshotNow, 
							0, 
							NULL);

	storeScan->tuple = NULL;
}

bool PersistentStore_GetNext(
	PersistentStoreScan			*storeScan,
	Datum						*values,
	ItemPointer					persistentTid,
	int64						*persistentSerialNum)
{
	ItemPointerData			previousFreeTid;

	storeScan->tuple = heap_getnext(storeScan->scan, ForwardScanDirection);
	if (storeScan->tuple == NULL)
		return false;

	PersistentStore_DeformTuple(
							storeScan->storeData,
							storeScan->persistentRel->rd_att,
							storeScan->tuple,
							values);
	PersistentStore_ExtractOurTupleData(
								storeScan->storeData,
								values,
								persistentSerialNum,
								&previousFreeTid);

	*persistentTid = storeScan->tuple->t_self;

	return true;
}

HeapTuple PersistentStore_GetScanTupleCopy(
	PersistentStoreScan			*storeScan)
{
	return heaptuple_copy_to(storeScan->tuple, NULL, NULL);
}

void PersistentStore_EndScan(
	PersistentStoreScan			*storeScan)
{
	heap_endscan(storeScan->scan);

	(*storeScan->storeData->closeRel)(storeScan->persistentRel);
}

// -----------------------------------------------------------------------------
// Helpers 
// -----------------------------------------------------------------------------

static XLogRecPtr nowaitXLogEndLoc;

inline static void XLogRecPtr_Zero(XLogRecPtr *xlogLoc)
{
	MemSet(xlogLoc, 0, sizeof(XLogRecPtr));
}

void PersistentStore_FlushXLog(void)
{
	if (nowaitXLogEndLoc.xlogid != 0 ||
		nowaitXLogEndLoc.xrecoff != 0)
	{
		XLogFlush(nowaitXLogEndLoc);
		XLogRecPtr_Zero(&nowaitXLogEndLoc);
	}
}

int64 PersistentStore_MyHighestSerialNum(
	PersistentStoreData 	*storeData)
{
	return storeData->myHighestSerialNum;
}

int64 PersistentStore_CurrentMaxSerialNum(
	PersistentStoreSharedData 	*storeSharedData)
{
	return storeSharedData->maxInUseSerialNum;
}

PersistentTidIsKnownResult PersistentStore_TidIsKnown(
	PersistentStoreSharedData 	*storeSharedData,
	ItemPointer 				persistentTid,
	ItemPointer 				maxTid)
{
	*maxTid = storeSharedData->maxTid;

	Assert(!PersistentStore_IsZeroTid(persistentTid));

	// UNDONE: I think the InRecovery test only applies to physical Master Mirroring on Standby.
	/* Only test this outside of recovery scenarios */
	if (Persistent_BeforePersistenceWork())
		return PersistentTidIsKnownResult_BeforePersistenceWork;

	if (storeSharedData->needToScanIntoSharedMemory)
		return PersistentTidIsKnownResult_ScanNotPerformedYet;

	if (PersistentStore_IsZeroTid(&storeSharedData->maxTid))
		return PersistentTidIsKnownResult_MaxTidIsZero;

	if (ItemPointerCompare(
						persistentTid,
						&storeSharedData->maxTid) <= 0) // Less-than or equal.
		return PersistentTidIsKnownResult_Known;
	else
		return PersistentTidIsKnownResult_NotKnown;
}

static void PersistentStore_DoInsertTuple(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData,
	Relation				persistentRel,
				/* The persistent table relation. */
	Datum					*values,
	bool					flushToXLog,
				/* When true, the XLOG record for this change will be flushed to disk. */
	ItemPointer 			persistentTid)
				/* TID of the stored tuple. */
{
	bool 		*nulls;
	HeapTuple	persistentTuple = NULL;
	XLogRecPtr	xlogInsertEndLoc;

	/*
	 * In order to keep the tuples the exact same size to enable direct reuse of
	 * free tuples, we do not use NULLs.
	 */
	nulls = (bool*)palloc0(storeData->numAttributes * sizeof(bool));
		
	/*
	 * Form the tuple.
	 */
	persistentTuple = heap_form_tuple(persistentRel->rd_att, values, nulls);
	if (!HeapTupleIsValid(persistentTuple))
		elog(ERROR, "Failed to build persistent tuple ('%s')",
		     storeData->tableName);

	/*
	 * (We have an exclusive lock (higher up) here so we can direct the insert
	 * to the last page.)
	 */
	{
		BlockNumber blockNumber;

		/*
		 * If we don't have a previous maxTid, we don't know the size of the
		 * table. Call RelationGetNumberOfBlocks() to find out.
		 */
		if (!ItemPointerIsValid(&storeSharedData->maxTid))
		{
			blockNumber = RelationGetNumberOfBlocks(persistentRel);
			if (blockNumber == 0)
				blockNumber = InvalidBlockNumber;
			else
				blockNumber--;
		}
		else
			blockNumber = ItemPointerGetBlockNumber(&storeSharedData->maxTid);
		
		frozen_heap_insert_directed(
							persistentRel, 
							persistentTuple,
							blockNumber);
	}

	if (Debug_persistent_store_print)
		elog(PersistentStore_DebugPrintLevel(), 
			 "PersistentStore_DoInsertTuple: old maximum known TID %s, new insert TID %s ('%s')",
			 ItemPointerToString(&storeSharedData->maxTid),
			 ItemPointerToString2(&persistentTuple->t_self),
			 storeData->tableName);
	if (ItemPointerCompare(
						&storeSharedData->maxTid,
						&persistentTuple->t_self) == -1)		
	{
		// Current max is Less-Than.
		storeSharedData->maxTid = persistentTuple->t_self;
	}
	
	/*
	 * Return the TID of the INSERT tuple.
	 * Return the XLOG location of the INSERT tuple's XLOG record.
	 */
	*persistentTid = persistentTuple->t_self;
		
	xlogInsertEndLoc = XLogLastInsertEndLoc();

	heap_freetuple(persistentTuple);

	if (flushToXLog)
	{
		XLogFlush(xlogInsertEndLoc);
		XLogRecPtr_Zero(&nowaitXLogEndLoc);
	}
	else
		nowaitXLogEndLoc = xlogInsertEndLoc;

	pfree(nulls);

}

static void PersistentStore_InsertTuple(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData,
	Datum					*values,
	bool					flushToXLog,
				/* When true, the XLOG record for this change will be flushed to disk. */
	ItemPointer 			persistentTid)
				/* TID of the stored tuple. */
{
	Relation	persistentRel;

#ifdef USE_ASSERT_CHECKING
	if (storeSharedData == NULL ||
		!PersistentStoreSharedData_EyecatcherIsValid(storeSharedData))
		elog(ERROR, "Persistent store shared-memory not valid");
#endif

	if (Debug_persistent_store_print)
		elog(PersistentStore_DebugPrintLevel(), 
			 "PersistentStore_InsertTuple: Going to insert new tuple ('%s', shared data %p)",
			 storeData->tableName,
			 storeSharedData);

	persistentRel = (*storeData->openRel)();

	PersistentStore_DoInsertTuple(
								storeData,
								storeSharedData,
								persistentRel,
								values,
								flushToXLog,
								persistentTid);

	(*storeData->closeRel)(persistentRel);
	
	if (Debug_persistent_store_print)
	{
		elog(PersistentStore_DebugPrintLevel(), 
			 "PersistentStore_InsertTuple: Inserted new tuple at TID %s ('%s')",
			 ItemPointerToString(persistentTid),
			 storeData->tableName);
		
		(*storeData->printTupleCallback)(
									PersistentStore_DebugPrintLevel(),
									"STORE INSERT TUPLE",
									persistentTid,
									values);
	}

}

void PersistentStore_UpdateTuple(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData,
	ItemPointer 			persistentTid,
				/* TID of the stored tuple. */
	Datum					*values,
	bool					flushToXLog)
				/* When true, the XLOG record for this change will be flushed to disk. */
{
	Relation	persistentRel;
	bool 		*nulls;
	HeapTuple	persistentTuple = NULL;
	XLogRecPtr 	xlogUpdateEndLoc;
	
#ifdef USE_ASSERT_CHECKING
	if (storeSharedData == NULL ||
		!PersistentStoreSharedData_EyecatcherIsValid(storeSharedData))
		elog(ERROR, "Persistent store shared-memory not valid");
#endif
	
	if (Debug_persistent_store_print)
		elog(PersistentStore_DebugPrintLevel(), 
			 "PersistentStore_ReplaceTuple: Going to update whole tuple at TID %s ('%s', shared data %p)",
			 ItemPointerToString(persistentTid),
			 storeData->tableName,
			 storeSharedData);

	persistentRel = (*storeData->openRel)();

	/*
	 * In order to keep the tuples the exact same size to enable direct reuse of
	 * free tuples, we do not use NULLs.
	 */
	nulls = (bool*)palloc0(storeData->numAttributes * sizeof(bool));
		
	/*
	 * Form the tuple.
	 */
	persistentTuple = heap_form_tuple(persistentRel->rd_att, values, nulls);
	if (!HeapTupleIsValid(persistentTuple))
		elog(ERROR, "Failed to build persistent tuple ('%s')",
		     storeData->tableName);

	persistentTuple->t_self = *persistentTid;

	frozen_heap_inplace_update(persistentRel, persistentTuple);

	/*
	 * Return the XLOG location of the UPDATE tuple's XLOG record.
	 */
	xlogUpdateEndLoc = XLogLastInsertEndLoc();

	heap_freetuple(persistentTuple);

	(*storeData->closeRel)(persistentRel);
	
	if (Debug_persistent_store_print)
	{
		elog(PersistentStore_DebugPrintLevel(), 
			 "PersistentStore_UpdateTuple: Updated whole tuple at TID %s ('%s')",
			 ItemPointerToString(persistentTid),
			 storeData->tableName);

		(*storeData->printTupleCallback)(
									PersistentStore_DebugPrintLevel(),
									"STORE UPDATED TUPLE",
									persistentTid,
									values);
	}

	if (flushToXLog)
	{
		XLogFlush(xlogUpdateEndLoc);
		XLogRecPtr_Zero(&nowaitXLogEndLoc);
	}
	else
		nowaitXLogEndLoc = xlogUpdateEndLoc;
}

void PersistentStore_ReplaceTuple(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData,
	ItemPointer 			persistentTid,
				/* TID of the stored tuple. */
	HeapTuple				tuple,
	Datum					*newValues,
	bool					*replaces,
	bool					flushToXLog)
				/* When true, the XLOG record for this change will be flushed to disk. */
{
	Relation	persistentRel;
	bool 		*nulls;
	HeapTuple	replacementTuple = NULL;
	XLogRecPtr 	xlogUpdateEndLoc;
	
#ifdef USE_ASSERT_CHECKING
	if (storeSharedData == NULL ||
		!PersistentStoreSharedData_EyecatcherIsValid(storeSharedData))
		elog(ERROR, "Persistent store shared-memory not valid");
#endif
	
	if (Debug_persistent_store_print)
		elog(PersistentStore_DebugPrintLevel(), 
			 "PersistentStore_ReplaceTuple: Going to replace set of columns in tuple at TID %s ('%s', shared data %p)",
			 ItemPointerToString(persistentTid),
			 storeData->tableName,
			 storeSharedData);

	persistentRel = (*storeData->openRel)();

	/*
	 * In order to keep the tuples the exact same size to enable direct reuse of
	 * free tuples, we do not use NULLs.
	 */
	nulls = (bool*)palloc0(storeData->numAttributes * sizeof(bool));
		
	/*
	 * Modify the tuple.
	 */
	replacementTuple = heap_modify_tuple(tuple, persistentRel->rd_att, 
										 newValues, nulls, replaces);

	replacementTuple->t_self = *persistentTid;
		
	frozen_heap_inplace_update(persistentRel, replacementTuple);

	/*
	 * Return the XLOG location of the UPDATE tuple's XLOG record.
	 */
	xlogUpdateEndLoc = XLogLastInsertEndLoc();

	heap_freetuple(replacementTuple);
	pfree(nulls);

	if (Debug_persistent_store_print)
	{
		Datum 			*readValues;
		bool			*readNulls;
		HeapTupleData 	readTuple;
		Buffer			buffer;
		HeapTuple		readTupleCopy;
		
		elog(PersistentStore_DebugPrintLevel(), 
			 "PersistentStore_ReplaceTuple: Replaced set of columns in tuple at TID %s ('%s')",
			 ItemPointerToString(persistentTid),
			 storeData->tableName);
		
		readValues = (Datum*)palloc(storeData->numAttributes * sizeof(Datum));
		readNulls = (bool*)palloc(storeData->numAttributes * sizeof(bool));

		readTuple.t_self = *persistentTid;
		
		if (!heap_fetch(persistentRel, SnapshotAny,
						&readTuple, &buffer, false, NULL))
		{
			elog(ERROR, "Failed to fetch persistent tuple at %s ('%s')",
				 ItemPointerToString(&readTuple.t_self),
				 storeData->tableName);
		}
		
		
		readTupleCopy = heaptuple_copy_to(&readTuple, NULL, NULL);
		
		ReleaseBuffer(buffer);
		
		heap_deform_tuple(readTupleCopy, persistentRel->rd_att, readValues, readNulls);
		
		(*storeData->printTupleCallback)(
									PersistentStore_DebugPrintLevel(),
									"STORE REPLACED TUPLE",
									persistentTid,
									readValues);

		heap_freetuple(readTupleCopy);
		pfree(readValues);
		pfree(readNulls);
	}

	(*storeData->closeRel)(persistentRel);
	
	if (flushToXLog)
	{
		XLogFlush(xlogUpdateEndLoc);
		XLogRecPtr_Zero(&nowaitXLogEndLoc);
	}
	else
		nowaitXLogEndLoc = xlogUpdateEndLoc;
}


void PersistentStore_ReadTuple(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData,
	ItemPointer					readTid,
	Datum						*values,
	HeapTuple					*tupleCopy)
{
	Relation	persistentRel;

	HeapTupleData 	tuple;
	Buffer			buffer;

	bool *nulls;
	
#ifdef USE_ASSERT_CHECKING
	if (storeSharedData == NULL ||
		!PersistentStoreSharedData_EyecatcherIsValid(storeSharedData))
		elog(ERROR, "Persistent store shared-memory not valid");
#endif
	
	if (Debug_persistent_store_print)
		elog(PersistentStore_DebugPrintLevel(), 
			 "PersistentStore_ReadTuple: Going to read tuple at TID %s ('%s', shared data %p)",
			 ItemPointerToString(readTid),
			 storeData->tableName,
			 storeSharedData);

	if (PersistentStore_IsZeroTid(readTid))
		elog(ERROR, "TID for fetch persistent tuple is invalid (0,0) ('%s')",
			 storeData->tableName);

	// UNDONE: I think the InRecovery test only applies to physical Master Mirroring on Standby.
	/* Only test this outside of recovery scenarios */
	if (!InRecovery 
		&& 
		(PersistentStore_IsZeroTid(&storeSharedData->maxTid)
		 ||
		 ItemPointerCompare(
						readTid,
						&storeSharedData->maxTid) == 1 // Greater-than.
		))
	{
		elog(ERROR, "TID %s for fetch persistent tuple is greater than the last known TID %s ('%s')",
			 ItemPointerToString(readTid),
			 ItemPointerToString2(&storeSharedData->maxTid),
			 storeData->tableName);
	}
	
	persistentRel = (*storeData->openRel)();

	tuple.t_self = *readTid;

	if (!heap_fetch(persistentRel, SnapshotAny,
					&tuple, &buffer, false, NULL))
	{
		elog(ERROR, "Failed to fetch persistent tuple at %s (maximum known TID %s, '%s')",
			 ItemPointerToString(&tuple.t_self),
			 ItemPointerToString2(&storeSharedData->maxTid),
			 storeData->tableName);
	}

	
	*tupleCopy = heaptuple_copy_to(&tuple, NULL, NULL);

	ReleaseBuffer(buffer);
	
	/*
	 * In order to keep the tuples the exact same size to enable direct reuse of
	 * free tuples, we do not use NULLs.
	 */
	nulls = (bool*)palloc(storeData->numAttributes * sizeof(bool));

	heap_deform_tuple(*tupleCopy, persistentRel->rd_att, values, nulls);

	(*storeData->closeRel)(persistentRel);
	
	if (Debug_persistent_store_print)
	{
		elog(PersistentStore_DebugPrintLevel(), 
			 "PersistentStore_ReadTuple: Successfully read tuple at TID %s ('%s')",
			 ItemPointerToString(readTid),
			 storeData->tableName);

		(*storeData->printTupleCallback)(
									PersistentStore_DebugPrintLevel(),
									"STORE READ TUPLE",
									readTid,
									values);
	}

	pfree(nulls);
}

/*
 * Check if the free TID is valid. If not, the free list is corrupted and we
 * pretend there are no free tuples to reset the free list. The corrupted free
 * list will be detached and cleaned during recovery or pt rebuild.
 */
static bool PersistentStore_ValidateFreeTID(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData,
	ItemPointerData				freeTid,
	ItemPointer					previousFreeTid,
	int64 						expectedPersistentSerialNum)
{
	Datum				*values;
	HeapTuple			tupleCopy;
	int64				persistentSerialNum;
	bool				tidIsValid = true;

	values = (Datum*)palloc(storeData->numAttributes * sizeof(Datum));
	PersistentStore_ReadTuple(
					storeData,
					storeSharedData,
					&freeTid,
					values,
					&tupleCopy);

	PersistentStore_ExtractOurTupleData(
								storeData,
								values,
								&persistentSerialNum,
								previousFreeTid);

	if (storeSharedData->maxFreeOrderNum == 1 && ItemPointerCompare(previousFreeTid, &freeTid) != 0)
	{
		tidIsValid = false;
		ereport(WARNING,
				(errmsg("integrity check for PT freeTid failed"),
				 errdetail("expected to have previous FreeTID at %s equal to itself (found %s, %s)",
						ItemPointerToString(&freeTid),
						ItemPointerToString2(previousFreeTid),
						storeData->tableName)));
	}

	if (PersistentStore_IsZeroTid(previousFreeTid))
	{
		tidIsValid = false;
		ereport(WARNING,
				(errmsg("integrity check for PT freeTid failed"),
				 errdetail("expected to have previous FreeTID at %s to be free (found %s, %s)",
						ItemPointerToString(&freeTid),
						ItemPointerToString2(previousFreeTid),
						storeData->tableName)));
	}

	if (persistentSerialNum != expectedPersistentSerialNum)
	{
		tidIsValid = false;
		ereport(WARNING,
				(errmsg("integrity check for PT freeTid failed"),
				 errdetail("expected persistent store tuple at %s to have order number " INT64_FORMAT " (found " INT64_FORMAT ", '%s')",
						ItemPointerToString(&freeTid),
						expectedPersistentSerialNum,
						persistentSerialNum,
						storeData->tableName)));
	}

	pfree(values);
	heap_freetuple(tupleCopy);

	// If the free TID is not valid, switch to a new free list here
	if (!tidIsValid)
	{
		storeSharedData->maxFreeOrderNum = 0;
		MemSet(&storeSharedData->freeTid, 0, sizeof(ItemPointerData));
		ereport(WARNING,
				(errmsg("switched to new free TID list")));
	}

	return tidIsValid;
}

static bool PersistentStore_GetFreeTuple(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData,
	ItemPointer				freeTid)
{
	ItemPointerData		previousFreeTid;

	MemSet(freeTid, 0, sizeof(ItemPointerData));

	if (Debug_persistent_store_print)
		elog(PersistentStore_DebugPrintLevel(), 
			 "PersistentStore_GetFreeTuple: Enter: maximum free order number " INT64_FORMAT ", free TID %s ('%s')",
			 storeSharedData->maxFreeOrderNum, 
			 ItemPointerToString(&storeSharedData->freeTid),
			 storeData->tableName);

	if (storeSharedData->maxFreeOrderNum == 0)
	{
		return false;	// No free tuples.
	}

	if (gp_persistent_skip_free_list)
	{
		if (Debug_persistent_store_print)
			elog(PersistentStore_DebugPrintLevel(), 
				 "PersistentStore_GetFreeTuple: Skipping because gp_persistent_skip_free_list GUC is ON ('%s')",
				 storeData->tableName);
		return false;	// Pretend no free tuples.
	}

	Assert(storeSharedData->freeTid.ip_posid != 0);

	if (!PersistentStore_ValidateFreeTID(
								storeData,
								storeSharedData,
								storeSharedData->freeTid,
								&previousFreeTid,
								storeSharedData->maxFreeOrderNum))
		return false;

	*freeTid = storeSharedData->freeTid;
	storeSharedData->maxFreeOrderNum--;
	storeSharedData->freeTid = previousFreeTid; /* previousFreeTid is set inside the ValidateFreeTID function */

	if (Debug_persistent_store_print)
		elog(PersistentStore_DebugPrintLevel(), 
			 "PersistentStore_GetFreeTuple: Exit: maximum free order number " INT64_FORMAT ", free TID %s ('%s')",
			 storeSharedData->maxFreeOrderNum, 
			 ItemPointerToString(&storeSharedData->freeTid),
			 storeData->tableName);

	if (validate_previous_free_tid &&
		storeSharedData->maxFreeOrderNum > 0 &&
		!PersistentStore_ValidateFreeTID(
										storeData,
										storeSharedData,
										storeSharedData->freeTid,
										&previousFreeTid,
										storeSharedData->maxFreeOrderNum))
		return false;

	return true;
}

void PersistentStore_AddTuple(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData,
	Datum					*values,
	bool					flushToXLog,
				/* When true, the XLOG record for this change will be flushed to disk. */
	ItemPointer 			persistentTid,
				/* TID of the stored tuple. */
	int64					*persistentSerialNum)
{
#ifdef USE_ASSERT_CHECKING
	if (storeSharedData == NULL ||
		!PersistentStoreSharedData_EyecatcherIsValid(storeSharedData))
		elog(ERROR, "Persistent store shared-memory not valid");
#endif

	PTCheck_BeforeAddingEntry(storeData, values);

	if (Debug_persistent_store_print)
		elog(PersistentStore_DebugPrintLevel(), 
			 "PersistentStore_AddTuple: Going to add tuple ('%s', shared data %p)",
			 storeData->tableName,
			 storeSharedData);

	*persistentSerialNum = ++storeSharedData->maxInUseSerialNum;
	storeData->myHighestSerialNum = storeSharedData->maxInUseSerialNum;

	GlobalSequence_Set(
				storeData->gpGlobalSequence,
				*persistentSerialNum);
	
	// Overwrite with the new serial number value.
	values[storeData->attNumPersistentSerialNum - 1] = 
										Int64GetDatum(*persistentSerialNum);

	if (PersistentStore_GetFreeTuple(
								storeData,
								storeSharedData,
								persistentTid))
	{
		Assert(persistentTid->ip_posid != 0);
		PersistentStore_UpdateTuple(
									storeData,
									storeSharedData,
									persistentTid,
									values,
									flushToXLog);
	}
	else
	{
		/*
		 * Add new tuple.
		 */

		PersistentStore_InsertTuple(
								storeData,
								storeSharedData,
								values,
								flushToXLog,
								persistentTid);
		Assert(persistentTid->ip_posid != 0);
	}

	storeSharedData->inUseCount++;

	if (Debug_persistent_store_print)
		elog(PersistentStore_DebugPrintLevel(), 
			 "PersistentStore_AddTuple: Added tuple ('%s', in use count " INT64_FORMAT ", shared data %p)",
			 storeData->tableName,
			 storeSharedData->inUseCount,
			 storeSharedData);
}

void PersistentStore_FreeTuple(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData,
	ItemPointer 			persistentTid,
				/* TID of the stored tuple. */
	Datum					*freeValues,
	bool					flushToXLog)
				/* When true, the XLOG record for this change will be flushed to disk. */
{
	Relation	persistentRel;
	HeapTuple	persistentTuple = NULL;
	ItemPointerData prevFreeTid;
	XLogRecPtr xlogEndLoc;
				/* The end location of the UPDATE XLOG record. */

	Assert( LWLockHeldByMe(PersistentObjLock) );
				
#ifdef USE_ASSERT_CHECKING
	if (storeSharedData == NULL ||
		!PersistentStoreSharedData_EyecatcherIsValid(storeSharedData))
		elog(ERROR, "Persistent store shared-memory not valid");
#endif
				
	if (Debug_persistent_store_print)
		elog(PersistentStore_DebugPrintLevel(), 
			 "PersistentStore_FreeTuple: Going to free tuple at TID %s ('%s', shared data %p)",
			 ItemPointerToString(persistentTid),
			 storeData->tableName,
			 storeSharedData);
	
	Assert(persistentTid->ip_posid != 0);

	persistentRel = (*storeData->openRel)();

	if (storeSharedData->maxFreeOrderNum > 0)
	{
		ItemPointerData		tmpPrevFreeTid;
		prevFreeTid = storeSharedData->freeTid;

		// Let us validate and have sanity check to make sure the prevFreeTid is really free.
		if (validate_previous_free_tid)
			PersistentStore_ValidateFreeTID(
									storeData,
									storeSharedData,
									prevFreeTid,
									&tmpPrevFreeTid,
									storeSharedData->maxFreeOrderNum);
	}

	storeSharedData->maxFreeOrderNum++;
	if (storeSharedData->maxFreeOrderNum == 1)
	{
		prevFreeTid = *persistentTid;		// So non-zero PreviousFreeTid indicates free.
	}
	storeSharedData->freeTid = *persistentTid;

	PersistentStore_FormTupleSetOurs(
							storeData,
							persistentRel->rd_att,
							freeValues,
							storeSharedData->maxFreeOrderNum,
							&prevFreeTid,
							&persistentTuple);

	persistentTuple->t_self = *persistentTid;
		
	frozen_heap_inplace_update(persistentRel, persistentTuple);

	/*
	 * XLOG location of the UPDATE tuple's XLOG record.
	 */
	xlogEndLoc = XLogLastInsertEndLoc();

	heap_freetuple(persistentTuple);

	(*storeData->closeRel)(persistentRel);

	storeSharedData->inUseCount--;

	if (Debug_persistent_store_print)
		elog(PersistentStore_DebugPrintLevel(), 
			 "PersistentStore_FreeTuple: Freed tuple at TID %s.  Maximum free order number " INT64_FORMAT ", in use count " INT64_FORMAT " ('%s')",
			 ItemPointerToString(&storeSharedData->freeTid),
			 storeSharedData->maxFreeOrderNum, 
			 storeSharedData->inUseCount,
			 storeData->tableName);

	if (flushToXLog)
	{
		XLogFlush(xlogEndLoc);
		XLogRecPtr_Zero(&nowaitXLogEndLoc);
	}
	else
		nowaitXLogEndLoc = xlogEndLoc;
}
