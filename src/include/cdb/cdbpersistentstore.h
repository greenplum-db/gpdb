/*-------------------------------------------------------------------------
 *
 * cdbpersistentstore.h
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbpersistentstore.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBPERSISTENTSTORE_H
#define CDBPERSISTENTSTORE_H

#include "miscadmin.h"
#include "access/persistentfilesysobjname.h"
#include "catalog/gp_global_sequence.h"
#include "storage/itemptr.h"
#include "storage/proc.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "utils/relcache.h"
#include "cdb/cdbfilerepprimary.h"
#include "cdb/cdbdirectopen.h"
#include "utils/guc.h"

/*
 * Can be used to suppress errcontext when doing low-level tracing where we
 * don't want "dynamic" errcontext like that used by the executor which executes a
 * lot of code to determine the errcontext string to recurse and cause stack overflow.
 */
#define SUPPRESS_ERRCONTEXT_DECLARE \
	ErrorContextCallback *suppressErrContext = NULL;

#define SUPPRESS_ERRCONTEXT_PUSH() \
{ \
	suppressErrContext = error_context_stack; \
	error_context_stack = NULL; \
}

#define SUPPRESS_ERRCONTEXT_POP() \
{ \
	error_context_stack = suppressErrContext; \
}


#define PersistentStoreSharedDataEyecatcher "PersistentStoreSharedData"

// Length includes 1 extra for NUL.
#define PersistentStoreSharedDataEyecatcher_Len 26

typedef struct PersistentStoreSharedData
{
	char	eyecatcher[PersistentStoreSharedDataEyecatcher_Len];
	bool				needToScanIntoSharedMemory;
	int64				maxInUseSerialNum;
		/* Value of persistent serial number that was assigned to last tuple */

	int64				inUseCount;
		/* Current number of tuples in persistent table */
} PersistentStoreSharedData;

inline static bool PersistentStoreSharedData_EyecatcherIsValid(
	PersistentStoreSharedData *storeSharedData)
{
	return (strcmp(storeSharedData->eyecatcher, PersistentStoreSharedDataEyecatcher) == 0);
}

typedef Relation (*PersistentStoreOpenRel) (void);
typedef void (*PersistentStoreCloseRel) (Relation relation);
typedef bool (*PersistentStoreScanTupleCallback) (
								ItemPointer 			persistentTid,
								int64					persistentSerialNum,
								Datum					*values);
typedef void (*PersistentStorePrintTupleCallback) (
								int						elevel,
								char					*prefix,
								ItemPointer 			persistentTid,
								Datum					*values);

#define PersistentStoreData_StaticInit {NULL,0,NULL,NULL,NULL,NULL,0,0}

typedef struct PersistentStoreData
{
	char								*tableName;
	GpGlobalSequence					gpGlobalSequence;
	PersistentStoreOpenRel				openRel;
	PersistentStoreCloseRel				closeRel;
	PersistentStoreScanTupleCallback	scanTupleCallback;
	PersistentStorePrintTupleCallback	printTupleCallback;
	int64			myHighestSerialNum;
	int				numAttributes;
	int				attNumPersistentSerialNum;
} PersistentStoreData;

typedef struct PersistentStoreScan
{
	PersistentStoreData 		*storeData;
	PersistentStoreSharedData 	*storeSharedData;
	Relation					persistentRel;
	HeapScanDesc				scan;
	HeapTuple					tuple;
} PersistentStoreScan;

inline static bool PersistentStore_IsZeroTid(
	ItemPointer		testTid)
{
	static ItemPointerData zeroTid = {{0,0},0};
	return (ItemPointerCompare(testTid, &zeroTid) == 0);
}


#ifdef USE_ASSERT_CHECKING

typedef struct MirroredLockLocalVars
{
	bool mirroredLockIsHeldByMe;
	bool specialResyncManagerFlag;
	bool mirroredVariablesSet;
} MirroredLockLocalVars;

#define MIRRORED_LOCK_DECLARE \
	MirroredLockLocalVars mirroredLockLocalVars = {false, false, false};
#else

typedef struct MirroredLockLocalVars
{
	bool mirroredLockIsHeldByMe;
	bool specialResyncManagerFlag;
} MirroredLockLocalVars;

#define MIRRORED_LOCK_DECLARE \
	MirroredLockLocalVars mirroredLockLocalVars = {false, false};
#endif

#ifdef USE_ASSERT_CHECKING
#define MIRRORED_LOCK \
	{ \
		mirroredLockLocalVars.mirroredLockIsHeldByMe = LWLockHeldByMe(MirroredLock); \
		mirroredLockLocalVars.specialResyncManagerFlag = FileRepPrimary_IsResyncManagerOrWorker(); \
		mirroredLockLocalVars.mirroredVariablesSet = true; \
		\
		if (!mirroredLockLocalVars.mirroredLockIsHeldByMe) \
		{ \
			if (!mirroredLockLocalVars.specialResyncManagerFlag) \
			{ \
				LWLockAcquire(MirroredLock , LW_SHARED); \
			} \
			else \
			{ \
				HOLD_INTERRUPTS(); \
			} \
		} \
		\
		Assert(InterruptHoldoffCount > 0); \
	}
#else
#define MIRRORED_LOCK \
	{ \
		mirroredLockLocalVars.mirroredLockIsHeldByMe = LWLockHeldByMe(MirroredLock); \
		mirroredLockLocalVars.specialResyncManagerFlag = FileRepPrimary_IsResyncManagerOrWorker(); \
		\
		if (!mirroredLockLocalVars.mirroredLockIsHeldByMe) \
		{ \
			if (!mirroredLockLocalVars.specialResyncManagerFlag) \
			{ \
				LWLockAcquire(MirroredLock , LW_SHARED); \
			} \
			else \
			{ \
				HOLD_INTERRUPTS(); \
			} \
		} \
		\
	}
#endif

#ifdef USE_ASSERT_CHECKING
#define MIRRORED_UNLOCK \
	{ \
		Assert(mirroredLockLocalVars.mirroredVariablesSet); \
		Assert(InterruptHoldoffCount > 0); \
		\
		if (!mirroredLockLocalVars.mirroredLockIsHeldByMe) \
		{ \
			if (!mirroredLockLocalVars.specialResyncManagerFlag) \
			{ \
				LWLockRelease(MirroredLock); \
			} \
			else \
			{ \
				RESUME_INTERRUPTS(); \
			} \
		} \
	}
#else
#define MIRRORED_UNLOCK \
	{ \
		if (!mirroredLockLocalVars.mirroredLockIsHeldByMe) \
		{ \
			if (!mirroredLockLocalVars.specialResyncManagerFlag) \
			{ \
				LWLockRelease(MirroredLock); \
			} \
			else \
			{ \
				RESUME_INTERRUPTS(); \
			} \
		} \
	}
#endif

#ifdef USE_ASSERT_CHECKING
#define MIRRORED_LOCK_BY_REF \
	{ \
		mirroredLockByRefVars->mirroredLockIsHeldByMe = LWLockHeldByMe(MirroredLock); \
		mirroredLockByRefVars->specialResyncManagerFlag = FileRepPrimary_IsResyncManagerOrWorker(); \
		mirroredLockByRefVars->mirroredVariablesSet = true; \
		\
		if (!mirroredLockByRefVars->mirroredLockIsHeldByMe) \
		{ \
			if (!mirroredLockByRefVars->specialResyncManagerFlag) \
			{ \
				LWLockAcquire(MirroredLock , LW_SHARED); \
			} \
			else \
			{ \
				HOLD_INTERRUPTS(); \
			} \
		} \
		\
		Assert(InterruptHoldoffCount > 0); \
	}
#else
#define MIRRORED_LOCK_BY_REF \
	{ \
		mirroredLockByRefVars->mirroredLockIsHeldByMe = LWLockHeldByMe(MirroredLock); \
		mirroredLockByRefVars->specialResyncManagerFlag = FileRepPrimary_IsResyncManagerOrWorker(); \
		\
		if (!mirroredLockByRefVars->mirroredLockIsHeldByMe) \
		{ \
			if (!mirroredLockByRefVars->specialResyncManagerFlag) \
			{ \
				LWLockAcquire(MirroredLock , LW_SHARED); \
			} \
			else \
			{ \
				HOLD_INTERRUPTS(); \
			} \
		} \
		\
	}
#endif

#ifdef USE_ASSERT_CHECKING
#define MIRRORED_UNLOCK_BY_REF \
	{ \
		Assert(mirroredLockByRefVars->mirroredVariablesSet); \
		Assert(InterruptHoldoffCount > 0); \
		\
		if (!mirroredLockByRefVars->mirroredLockIsHeldByMe) \
		{ \
			if (!mirroredLockByRefVars->specialResyncManagerFlag) \
			{ \
				LWLockRelease(MirroredLock); \
			} \
			else \
			{ \
				RESUME_INTERRUPTS(); \
			} \
		} \
	}
#else
#define MIRRORED_UNLOCK_BY_REF \
	{ \
		if (!mirroredLockByRefVars->mirroredLockIsHeldByMe) \
		{ \
			if (!mirroredLockByRefVars->specialResyncManagerFlag) \
			{ \
				LWLockRelease(MirroredLock); \
			} \
			else \
			{ \
				RESUME_INTERRUPTS(); \
			} \
		} \
	}
#endif

/*
 * Helper DEFINEs for the Persistent state-change modules to READ or WRITE.
 * We must maintain proper lock acquisition and ordering to prevent any
 * possible deadlocks.
 *
 * The WRITE_PERSISTENT_STATE_ORDERED_LOCK gets these locks:
 *    MirroredLock        SHARED
 *    PersistentObjLock   EXCLUSIVE
 *
 * The READ_PERSISTENT_STATE_ORDERED_LOCK gets this lock:
 *    PersistentObjLock   SHARED
 *
 * By taking a SHARED MirroredLock, the process is blocking the filerep resync logic
 * from taking an EXCLUSIVE MirroredLock, there by preventing both a mirror write and
 * persistent table I/O at the same time. The filerep logic will always wait for an
 * EXCLUSIVE lock.
 *
 * By taking an exclusive PersistentObjLock, the process is preventing simultaneous
 * I/O on the persistent tables. Only one process can obtain an EXCLUSIVE
 * PersistentObjLock.
 *
 * WRITE_PERSISTENT_STATE_ORDERED_LOCK also sets MyProc->inCommit, to block a checkpoint
 * from starting while performing persistent table I/O.
 *
 * NOTE: Below these locks are the Buffer Pool content locks.
 */


/*
 * WRITE
 */

typedef struct WritePersistentStateLockLocalVars
{
	bool persistentObjLockIsHeldByMe;
	bool save_inCommit;
} WritePersistentStateLockLocalVars;

#define WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE \
	MIRRORED_LOCK_DECLARE; \
	WritePersistentStateLockLocalVars writePersistentStateLockLocalVars;

#define WRITE_PERSISTENT_STATE_ORDERED_LOCK \
	{ \
		MIRRORED_LOCK; \
		writePersistentStateLockLocalVars.save_inCommit = MyProc->inCommit; \
		MyProc->inCommit = true;				\
		writePersistentStateLockLocalVars.persistentObjLockIsHeldByMe = LWLockHeldByMe(PersistentObjLock); \
		if (!writePersistentStateLockLocalVars.persistentObjLockIsHeldByMe) \
		{ \
			LWLockAcquire(PersistentObjLock , LW_EXCLUSIVE); \
		} \
		START_CRIT_SECTION(); \
	}

#define WRITE_PERSISTENT_STATE_ORDERED_UNLOCK \
	{ \
		MIRRORED_UNLOCK; \
		MyProc->inCommit = writePersistentStateLockLocalVars.save_inCommit; \
		if (!writePersistentStateLockLocalVars.persistentObjLockIsHeldByMe) \
		{ \
			LWLockRelease(PersistentObjLock); \
		} \
		END_CRIT_SECTION(); \
	}

/*
 * READ
 */

typedef struct ReadPersistentStateLockLocalVars
{
	bool persistentObjLockIsHeldByMe;
} ReadPersistentStateLockLocalVars;

#define READ_PERSISTENT_STATE_ORDERED_LOCK_DECLARE \
	ReadPersistentStateLockLocalVars readPersistentStateLockLocalVars;

#define READ_PERSISTENT_STATE_ORDERED_LOCK \
	{ \
		readPersistentStateLockLocalVars.persistentObjLockIsHeldByMe = LWLockHeldByMe(PersistentObjLock); \
		if (!readPersistentStateLockLocalVars.persistentObjLockIsHeldByMe) \
		{ \
			LWLockAcquire(PersistentObjLock , LW_SHARED); \
		} \
	}

#define READ_PERSISTENT_STATE_ORDERED_UNLOCK \
	{ \
		if (!readPersistentStateLockLocalVars.persistentObjLockIsHeldByMe) \
		{ \
			LWLockRelease(PersistentObjLock); \
		} \
	}


inline static bool Persistent_BeforePersistenceWork(void)
{
	if (IsBootstrapProcessingMode())
		return true;

	if (gp_before_persistence_work)
		return true;

	return false;
}

inline static int PersistentStore_DebugPrintLevel(void)
{
	if (Debug_persistent_bootstrap_print && IsBootstrapProcessingMode())
		return WARNING;
	else
		return Debug_persistent_store_print_level;
}

extern void PersistentStore_InitShared(
	PersistentStoreSharedData 	*storeSharedData);

extern void PersistentStore_Init(
	PersistentStoreData 		*storeData,
	char						*tableName,
	GpGlobalSequence			gpGlobalSequence,
	PersistentStoreOpenRel		openRel,
	PersistentStoreCloseRel		closeRel,
	PersistentStoreScanTupleCallback	scanTupleCallback,
	PersistentStorePrintTupleCallback	printTupleCallback,
	int 						numAttributes,
	int 						attNumPersistentSerialNum);

extern void PersistentStore_InitScanUnderLock(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData);

extern void PersistentStore_Scan(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData	*storeSharedData,
	PersistentStoreScanTupleCallback	scanTupleCallback);

extern void PersistentStore_BeginScan(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData	*storeSharedData,
	PersistentStoreScan			*storeScan);

extern void PersistentStore_DeformTuple(
	PersistentStoreData 	*storeData,
	TupleDesc				tupleDesc,			
						/* tuple descriptor */
    HeapTuple 				tuple,
	Datum					*values);

extern bool PersistentStore_GetNext(
	PersistentStoreScan			*storeScan,
	Datum						*values,
	ItemPointer					persistentTid,
	int64						*persistentSerialNum);

extern HeapTuple PersistentStore_GetScanTupleCopy(
	PersistentStoreScan			*storeScan);

extern void PersistentStore_EndScan(
	PersistentStoreScan			*storeScan);

extern void PersistentStore_FlushXLog(void);

extern int64 PersistentStore_MyHighestSerialNum(
	PersistentStoreData 	*storeData);

extern int64 PersistentStore_CurrentMaxSerialNum(
	PersistentStoreSharedData 	*storeSharedData);

typedef enum PersistentTidIsKnownResult
{
	PersistentTidIsKnownResult_None = 0,
	PersistentTidIsKnownResult_BeforePersistenceWork,
	PersistentTidIsKnownResult_ScanNotPerformedYet,
	PersistentTidIsKnownResult_MaxTidIsZero,
	PersistentTidIsKnownResult_Known,
	PersistentTidIsKnownResult_NotKnown,
	MaxPersistentTidIsKnownResult		/* must always be last */
} PersistentTidIsKnownResult;

extern void PersistentStore_UpdateTuple(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData	*storeSharedData,
	ItemPointer 			persistentTid,
				/* TID of the stored tuple. */
	Datum					*values,
	bool					flushToXLog);
				/* When true, the XLOG record for this change will be flushed to disk. */

extern void PersistentStore_ReplaceTuple(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData	*storeSharedData,
	ItemPointer 			persistentTid,
				/* TID of the stored tuple. */
	HeapTuple				tuple,
	Datum					*newValues,
	bool					*replaces,
	bool					flushToXLog);
				/* When true, the XLOG record for this change will be flushed to disk. */

extern void PersistentStore_ReadTuple(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData,
	ItemPointer					readTid,
	Datum						*values,
	HeapTuple					*tupleCopy);

extern void PersistentStore_AddTuple(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData,
	Datum					*values,
	bool					flushToXLog,
				/* When true, the XLOG record for this change will be flushed to disk. */
	ItemPointer 			persistentTid,
				/* TID of the stored tuple. */
	int64					*persistentSerialNum);

extern void PersistentStore_FreeTuple(
	PersistentStoreData 		*storeData,
	PersistentStoreSharedData 	*storeSharedData,
	ItemPointer 			persistentTid,
				/* TID of the stored tuple. */
	Datum					*freeValues,
	bool					flushToXLog);
				/* When true, the XLOG record for this change will be flushed to disk. */

#endif   /* CDBPERSISTENTSTORE_H */

