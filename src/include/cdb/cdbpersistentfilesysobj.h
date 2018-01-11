/*-------------------------------------------------------------------------
 *
 * cdbpersistentfilesysobj.h
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbpersistentfilesysobj.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBPERSISTENTFILESYSOBJ_H
#define CDBPERSISTENTFILESYSOBJ_H

#include "miscadmin.h"
#include "utils/guc.h"
#include "access/persistentfilesysobjname.h"
#include "access/persistentendxactrec.h"
#include "cdb/cdbpersistentstore.h"
#include "catalog/gp_persistent.h"

typedef struct PersistentFileSysObjSharedData
{
	PersistentStoreSharedData	storeSharedData;
} PersistentFileSysObjSharedData;

#define PersistentFileSysObjData_StaticInit {0,PersistentStoreData_StaticInit,0,0,0}

typedef struct PersistentFileSysObjData
{
	PersistentFsObjType		fsObjType;
	PersistentStoreData		storeData;
	int 					attNumPersistentState;
	int 					attNumMirrorExistenceState;
	int 					attNumParentXid;
} PersistentFileSysObjData;

inline static int Persistent_DebugPrintLevel(void)
{
	if (Debug_persistent_bootstrap_print && IsBootstrapProcessingMode())
		return WARNING;
	else
		return Debug_persistent_print_level;
}

extern void PersistentFileSysObj_InitShared(
	PersistentFileSysObjSharedData 	*fileSysObjSharedData);

extern void PersistentFileSysObj_Init(
	PersistentFileSysObjData 			*fileSysObjData,
	PersistentFileSysObjSharedData 		*fileSysObjSharedData,
	PersistentFsObjType					fsObjType,
	PersistentStoreScanTupleCallback	scanTupleCallback);

extern void PersistentFileSysObj_Reset(void);

extern void PersistentFileSysObj_GetDataPtrs(
	PersistentFsObjType				fsObjType,
	PersistentFileSysObjData		**fileSysObjData,
	PersistentFileSysObjSharedData	**fileSysObjSharedData);

extern void PersistentFileSysObj_BuildInitScan(void);

extern void PersistentFileSysObj_StartupInitScan(void);

extern void PersistentFileSysObj_VerifyInitScan(void);

extern void PersistentFileSysObj_Scan(
	PersistentFsObjType			fsObjType,
	PersistentStoreScanTupleCallback	scanTupleCallback);

extern void PersistentFileSysObj_FlushXLog(void);

extern int64 PersistentFileSysObj_MyHighestSerialNum(
	PersistentFsObjType 	fsObjType);
extern int64 PersistentFileSysObj_CurrentMaxSerialNum(
	PersistentFsObjType 	fsObjType);

extern void PersistentFileSysObj_UpdateTuple(
	PersistentFsObjType		fsObjType,
	ItemPointer 			persistentTid,
				/* TID of the stored tuple. */
	Datum 					*values,
	bool					flushToXLog);
				/* When true, the XLOG record for this change will be flushed to disk. */

extern void PersistentFileSysObj_ReplaceTuple(
	PersistentFsObjType 	fsObjType,
	ItemPointer 			persistentTid,
				/* TID of the stored tuple. */
	HeapTuple				tuple,
	Datum					*newValues,
	bool					*replaces,
	bool					flushToXLog);
				/* When true, the XLOG record for this change will be flushed to disk. */

extern void PersistentFileSysObj_ReadTuple(
	PersistentFsObjType 		fsObjType,
	ItemPointer					readTid,
	Datum						*values,
	HeapTuple					*tupleCopy);

extern void PersistentFileSysObj_AddTuple(
	PersistentFsObjType			fsObjType,
	Datum						*values,
	bool						flushToXLog,
				/* When true, the XLOG record for this change will be flushed to disk. */
	ItemPointer 				persistentTid,
				/* TID of the stored tuple. */
	int64						*persistentSerialNum);

extern void PersistentFileSysObj_FreeTuple(
	PersistentFileSysObjData		*fileSysObjData,
	PersistentFileSysObjSharedData	*fileSysObjSharedData,
	PersistentFsObjType 		fsObjType,
	ItemPointer 				persistentTid,
				/* TID of the stored tuple. */
	bool						flushToXLog);
				/* When true, the XLOG record for this change will be flushed to disk. */


/*
 * The file kinds of a persistent file-system object verification results.
 */
typedef enum PersistentFileSysObjVerifyExpectedResult
{
	PersistentFileSysObjVerifyExpectedResult_None = 0,
	PersistentFileSysObjVerifyExpectedResult_DeleteUnnecessary = 1,
	PersistentFileSysObjVerifyExpectedResult_StateChangeAlreadyDone = 2,
	PersistentFileSysObjVerifyExpectedResult_StateChangeNeeded = 3,
	PersistentFileSysObjVerifyExpectedResult_ErrorSuppressed = 4,
	MaxPersistentFileSysObjVerifyExpectedResult /* must always be last */
} PersistentFileSysObjVerifyExpectedResult;

typedef void (*PersistentFileSysObjVerifiedActionCallback) (
											PersistentFileSysObjName 	*fsObjName,
											ItemPointer 				persistentTid,
											int64						persistentSerialNum,
											PersistentFileSysObjVerifyExpectedResult verifyExpectedResult,
											PersistentFileSysRelStorageMgr relStorageMgr);

/*
 * The file kinds of a persistent file-system object state-change results.
 */
typedef enum PersistentFileSysObjStateChangeResult
{
	PersistentFileSysObjStateChangeResult_None = 0,
	PersistentFileSysObjStateChangeResult_DeleteUnnecessary = 1,
	PersistentFileSysObjStateChangeResult_StateChangeOk = 2,
	PersistentFileSysObjStateChangeResult_ErrorSuppressed = 3,
	MaxPersistentFileSysObjStateChangeResult /* must always be last */
} PersistentFileSysObjStateChangeResult;

extern char *PersistentFileSysObjStateChangeResult_Name(
	PersistentFileSysObjStateChangeResult result);

extern PersistentFileSysObjStateChangeResult PersistentFileSysObj_StateChange(
	PersistentFileSysObjName	*fsObjName,
	ItemPointer 				persistentTid,
	int64						persistentSerialNum,
	PersistentFileSysState		nextState,
	bool						retryPossible,
	bool						flushToXLog,
	PersistentFileSysState		*oldState,
	PersistentFileSysObjVerifiedActionCallback verifiedActionCallback);

extern void PersistentFileSysObj_RepairDelete(
		PersistentFsObjType 		fsObjType,
		ItemPointer 				persistentTid);

extern void PersistentFileSysObj_Created(
	PersistentFileSysObjName	*fsObjName,
	ItemPointer 			persistentTid,
	int64					persistentSerialNum,
	bool					retryPossible);

extern PersistentFileSysObjStateChangeResult PersistentFileSysObj_MarkAbortingCreate(
	PersistentFileSysObjName	*fsObjName,
	ItemPointer 			persistentTid,
	int64					persistentSerialNum,
	bool					retryPossible);

extern PersistentFileSysObjStateChangeResult PersistentFileSysObj_MarkDropPending(
	PersistentFileSysObjName	*fsObjName,
	ItemPointer 			persistentTid,
	int64					persistentSerialNum,
	bool					retryPossible);

extern void PersistentFileSysObj_DropObject(
	PersistentFileSysObjName	*fsObjName,
	PersistentFileSysRelStorageMgr relStorageMgr,
	char						*relationName,
					/* For tracing only.  Can be NULL in some execution paths. */
	ItemPointer 				persistentTid,
	int64						persistentSerialNum,
	bool 						ignoreNonExistence,
	bool						debugPrint,
	int							debugPrintLevel);

extern void PersistentFileSysObj_EndXactDrop(
	PersistentFileSysObjName	*fsObjName,
	PersistentFileSysRelStorageMgr relStorageMgr,
	char						*relationName,
					/* For tracing only.  Can be NULL in some execution paths. */
	ItemPointer 				persistentTid,
	int64						persistentSerialNum,
	bool						ignoreNonExistence);

extern void PersistentFileSysObj_PreparedEndXactAction(
	TransactionId 					preparedXid,
	const char 						*gid,
	PersistentEndXactRecObjects 	*persistentObjects,
	bool							isCommit,
	int								prepareAppendOnlyIntentCount);

extern void PersistentFileSysObj_ChangeMirrorState(
	PersistentFsObjType 			fsObjType,
	ItemPointer 					persistentTid,
	int64							persistentSerialNum,
	MirroredObjectExistenceState	mirrorExistenceState,
	MirroredRelDataSynchronizationState relDataSynchronizationState,
	bool							flushToXLog);

extern void PersistentFileSysObj_MarkBufPoolRelationForScanIncrementalResync(
	PersistentFileSysObjName 	*fsObjName,
	ItemPointer					persistentTid,
	int64						persistentSerialNum,
	bool						flushToXLog);

extern void PersistentFileSysObj_UpdateAppendOnlyMirrorResyncEofs(
	RelFileNode					*relFileNode,
	int32						segmentFileNum,
	ItemPointer 				persistentTid,
	int64						persistentSerialNum,
	bool						mirrorCatchupRequired,
	int64						mirrorNewEof,
	bool						recovery,
	bool						flushToXLog);

extern bool PersistentFileSysObj_CanAppendOnlyCatchupDuringResync(
		RelFileNode 				*relFileNode,
		int32						segmentFileNum,
		ItemPointer 				persistentTid,
		int64						persistentSerialNum,
		int64						*eof);

extern void PersistentFileSysObj_GetAppendOnlyCatchupMirrorStartEof(
	RelFileNode					*relFileNode,
	int32						segmentFileNum,
	ItemPointer 				persistentTid,
	int64						persistentSerialNum,
	int64						*startEof);

extern void PersistentFileSysObj_RequestResynchronizeTransition(void);

extern void PersistentFileSysObj_MarkWholeMirrorFullCopy(void);

extern void PersistentFileSysObj_MarkAppendOnlyCatchup(void);

extern void PersistentFileSysObj_MarkSpecialScanIncremental(void);

extern void PersistentFileSysObj_MarkPageIncrementalFromChangeLog(void);

extern void PersistentFileSysObj_MirrorReCreate(void);

extern void PersistentFileSysObj_MarkMirrorReCreated(void);

extern void PersistentFileSysObj_MirrorReDrop(void);

extern void PersistentFileSysObj_MarkMirrorReDropped(void);

typedef struct ResynchronizeScanToken
{
	bool				beginScan;
	PersistentStoreScan storeScan;
	bool				done;
} ResynchronizeScanToken;

inline static void ResynchronizeScanToken_Init(
	ResynchronizeScanToken		*token)
{
	MemSet(token, 0, sizeof(ResynchronizeScanToken));
	token->beginScan = false;
	token->done = false;
}

extern bool PersistentFileSysObj_ResynchronizeScan(
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
	int64							*persistentSerialNum);

/*
 * Refetch the resynchronize relation information while under RELATION_RESYNCHRONIZE lock
 * based on its persistent TID and serial number.
 */
extern bool PersistentFileSysObj_ResynchronizeRefetch(
	RelFileNode						*relFileNode,
	int32							*segmentFileNum,
	ItemPointer						persistentTid,
	int64							persistentSerialNum,
	PersistentFileSysRelStorageMgr 	*relStorageMgr,
	MirroredRelDataSynchronizationState *mirrorDataSynchronizationState,
	XLogRecPtr						*mirrorBufpoolResyncCkptLoc,
	BlockNumber 					*mirrorBufpoolResyncCkptBlockNum,
	int64							*mirrorAppendOnlyLossEof,
	int64							*mirrorAppendOnlyNewEof);

extern void PersistentFileSysObj_ResynchronizeRelationComplete(
	ItemPointer 					persistentTid,
	int64							persistentSerialNum,
	int64							mirrorLossEof,
	bool							flushToXLog);

typedef struct OnlineVerifyScanToken
{
	bool				beginScan;
	PersistentStoreScan storeScan;
	bool				done;
} OnlineVerifyScanToken;

inline static void OnlineVerifyScanToken_Init(
	OnlineVerifyScanToken		*token)
{
	MemSet(token, 0, sizeof(OnlineVerifyScanToken));
	token->beginScan = false;
	token->done = false;
}

typedef struct FilespaceScanToken
{
	bool				beginScan;
	PersistentStoreScan storeScan;
	bool				done;
} FilespaceScanToken;

inline static void FilespaceScanToken_Init(
	FilespaceScanToken		*token)
{
	MemSet(token, 0, sizeof(FilespaceScanToken));
	token->beginScan = false;
	token->done = false;
}

inline static int64 PersistentFileSysObj_ReadEof(
		PersistentFsObjType 	fsObjType,
		ItemPointer             persistentTid)
{
		Datum                   values[Natts_gp_persistent_relation_node];
		HeapTuple               tupleCopy;
		int64                   persistentEof;

		/* Get EOF from gp_persistent_relation_node */
		PersistentFileSysObj_ReadTuple(
									   fsObjType,
									   persistentTid,
									   values,
									   &tupleCopy);
		persistentEof = DatumGetInt64(values[Anum_gp_persistent_relation_node_mirror_append_only_new_eof - 1]);
		heap_freetuple(tupleCopy);
		return persistentEof;
}

extern bool PersistentFileSysObj_ScanForRelation(
	RelFileNode 		*relFileNode,
				/* The tablespace, database, and relation OIDs for the create. */
	int32				segmentFileNum,
	ItemPointer 		persistentTid,
				/* Resulting TID of the gp_persistent_rel_files tuple for the relation. */
	int64				*persistentSerialNum);
				/* Resulting serial number for the relation.  Distinquishes the uses of the tuple. */

extern void PersistentFileSysObj_StartupIntegrityCheck(void);

extern Size PersistentFileSysObj_ShmemSize(void);

extern void PersistentFileSysObj_ShmemInit(void);

extern void PersistentFileSysObj_AddMirror(PersistentFileSysObjName *fsObjName,
										   ItemPointer persistentTid,
										   int64 persistentSerialNum,
										   int16 pridbid,
										   int16 mirdbid,
										   void *arg,
										   bool set_mirror_existence,
										   bool flushToXLog);

extern void PersistentFileSysObj_RemoveSegment(
								   PersistentFileSysObjName *fsObjName,
								   ItemPointer persistentTid,
								   int64 persistentSerialNum,
								   int16 dbid,
								   bool ismirror,
								   bool flushToXLog);

extern void PersistentFileSysObj_ActivateStandby(
								   PersistentFileSysObjName *fsObjName,
								   ItemPointer persistentTid,
								   int64 persistentSerialNum,
								   int16 newmaster,
								   int16 oldmaster,
								   bool flushToXLog);

extern void PersistentFileSysObj_DoGlobalSequenceScan(void);

extern int PersistentFileSysObj_CountResyncObjects(void);

#endif   /* CDBPERSISTENTFILESYSOBJ_H */
