/*-------------------------------------------------------------------------
 *
 * cdbpersistentstore.c
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbpersistentstore.c
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
#include "utils/faultinjector.h"
#include "utils/guc.h"
#include "utils/syscache.h"
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
	int 						attNumPersistentSerialNum)
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
	int64					*persistentSerialNum)
{
	*persistentSerialNum = DatumGetInt64(values[storeData->attNumPersistentSerialNum - 1]);
}

static void PersistentStore_DoInitScan(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData)
{
	PersistentStoreScan 	storeScan;
	ItemPointerData			persistentTid;
	int64					persistentSerialNum;
	Datum					*values;
	int64					globalSequenceNum;

	values = (Datum*)palloc(storeData->numAttributes * sizeof(Datum));

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
		PersistentStore_ExtractOurTupleData(
									storeData,
									values,
									&persistentSerialNum);

		if (Debug_persistent_recovery_print)
			(*storeData->printTupleCallback)(
										PersistentRecovery_DebugPrintLevel(),
										"SCAN",
										&persistentTid,
										values);

		storeSharedData->inUseCount++;

		if (storeSharedData->maxInUseSerialNum < persistentSerialNum)
		{
			storeSharedData->maxInUseSerialNum = persistentSerialNum;
			storeData->myHighestSerialNum = storeSharedData->maxInUseSerialNum;
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
		 * Use the gp_persistent_repair_global_sequence GUC to get the
		 * system up.
		 */

		if (gp_persistent_repair_global_sequence)
		{
			elog(LOG, "need to repair global sequence number " INT64_FORMAT
				 " so use scanned maximum value " INT64_FORMAT " ('%s')",
				 globalSequenceNum,
				 storeSharedData->maxInUseSerialNum,
				 storeData->tableName);
		}
		else
		{
			elog(ERROR, "global sequence number " INT64_FORMAT " less than "
				 "maximum value " INT64_FORMAT " found in scan ('%s')",
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
			 "PersistentStore_DoInitScan ('%s'): maximum in-use serial number "
			 INT64_FORMAT ,
			 storeData->tableName,
			 storeSharedData->maxInUseSerialNum);
}

void PersistentStore_InitScanUnderLock(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData)
{
	if (!storeSharedData->needToScanIntoSharedMemory)
	{
		/* Someone else got in first. */
		return;
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
								persistentSerialNum);

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

	frozen_heap_insert(
					persistentRel,
					persistentTuple);

	if (Debug_persistent_store_print)
		elog(PersistentStore_DebugPrintLevel(), 
			 "PersistentStore_DoInsertTuple: new insert TID %s ('%s')",
			 ItemPointerToString2(&persistentTuple->t_self),
			 storeData->tableName);
	
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

	persistentRel = (*storeData->openRel)();

	tuple.t_self = *readTid;

	if (heap_fetch(persistentRel, SnapshotAny,
					&tuple, &buffer, false, NULL))
	{
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
	else
	{
		*tupleCopy = NULL;
	}
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

	/*
	 * Add new tuple.
	 */

	PersistentStore_InsertTuple(
							storeData,
							storeSharedData,
							values,
							flushToXLog,
							persistentTid);
	Assert(ItemPointerIsValid(persistentTid));

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
	
	Assert(ItemPointerIsValid(persistentTid));

	persistentRel = (*storeData->openRel)();
	simple_heap_delete_xid(persistentRel, persistentTid, FrozenTransactionId);
	/*
	 * XLOG location of the UPDATE tuple's XLOG record.
	 */
	xlogEndLoc = XLogLastInsertEndLoc();

	(*storeData->closeRel)(persistentRel);

	storeSharedData->inUseCount--;

	if (flushToXLog)
	{
		XLogFlush(xlogEndLoc);
		XLogRecPtr_Zero(&nowaitXLogEndLoc);
	}
	else
		nowaitXLogEndLoc = xlogEndLoc;
}
