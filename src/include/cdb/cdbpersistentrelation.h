/*-------------------------------------------------------------------------
 *
 * cdbpersistentrelation.h
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbpersistentrelation.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBPERSISTENTRELATION_H
#define CDBPERSISTENTRELATION_H

#include "access/persistentfilesysobjname.h"
#include "cdb/cdbpersistentfilesysobj.h"

/*
 * This module is for generic relation file create and drop.
 *
 * For create, it makes the file-system create of an empty file fully transactional so
 * the relation file will be deleted even on system crash.  The relation file could be a heap,
 * index, or append-only (row- or column-store).
 */
extern int64 PersistentRelation_MyHighestSerialNum(void);
extern int64 PersistentRelation_CurrentMaxSerialNum(void);

extern void PersistentRelation_CheckTablespace(
	Oid 			tablespace,
	int32			*useCount,
	RelFileNode 	*exampleRelFileNode);

extern void PersistentRelation_Reset(void);

/*
 * Indicate we intend to create a relation file as part of the current transaction.
 *
 * An XLOG IntentToCreate record is generated that will guard the subsequent file-system
 * create in case the transaction aborts.
 *
 * After 1 or more calls to this routine to mark intention about relation files that are going
 * to be created, call ~_DoPendingCreates to do the actual file-system creates.  (See its
 * note on XLOG flushing).
 */
extern void PersistentRelation_AddCreatePending(
	RelFileNode 		*relFileNode,
				/* The tablespace, database, and relation OIDs for the create. */
	int32				segmentFileNum,
	PersistentFileSysRelStorageMgr relStorageMgr,
	PersistentFileSysRelBufpoolKind relBufpoolKind,
	bool				bufferPoolBulkLoad,
	MirroredObjectExistenceState mirrorExistenceState,
	MirroredRelDataSynchronizationState relDataSynchronizationState,
	char				*relationName,
	ItemPointer 		persistentTid,
				/* Resulting TID of the gp_persistent_rel_files tuple for the relation. */
	int64				*persistentSerialNum,
				/* Resulting serial number for the relation.  Distinquishes the uses of the tuple. */
	bool				flushToXLog,
				/* When true, the XLOG record for this change will be flushed to disk. */
	bool				isLocalBuf);
	
extern void PersistentRelation_AddCreated(
	RelFileNode 		*relFileNode,
				/* The tablespace, database, and relation OIDs for the create. */
	int32				segmentFileNum,
	PersistentFileSysRelStorageMgr relStorageMgr,
	PersistentFileSysRelBufpoolKind relBufpoolKind,
	MirroredObjectExistenceState mirrorExistenceState,
	MirroredRelDataSynchronizationState relDataSynchronizationState,
	int64				mirrorAppendOnlyLossEof,
	int64				mirrorAppendOnlyNewEof,
	char				*relationName,
	ItemPointer 		persistentTid,
				/* Resulting TID of the gp_persistent_rel_files tuple for the relation. */
	int64				*persistentSerialNum,
				/* Resulting serial number for the relation.  Distinquishes the uses of the tuple. */
	bool				flushToXLog);
				/* When true, the XLOG record for this change will be flushed to disk. */
				
// -----------------------------------------------------------------------------
// Transaction End	
// -----------------------------------------------------------------------------

void PersistentRelation_FinishBufferPoolBulkLoad(
	RelFileNode 		*relFileNode,
				/* The tablespace, database, and relation OIDs for the created relation. */
	ItemPointer 		persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the relation. */
	int64				persistentSerialNum);
				/* Serial number for the relation.	Distinquishes the uses of the tuple. */

/*
 * Indicate the transaction commited and the relation is officially created.
 */
extern void PersistentRelation_Created(
	RelFileNode 		*relFileNode,
				/* The tablespace, database, and relation OIDs for the created relation. */
	int32				segmentFileNum,
	ItemPointer 		persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the relation. */
	int64				persistentSerialNum,
				/* Serial number for the relation.	Distinquishes the uses of the tuple. */
	bool				retryPossible);

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
extern PersistentFileSysObjStateChangeResult PersistentRelation_MarkDropPending(
	RelFileNode 		*relFileNode,
				/* The tablespace, database, and relation OIDs for the drop. */
	int32				segmentFileNum,
	ItemPointer 		persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the relation. */
	int64				persistentSerialNum,
				/* Serial number for the relation.	Distinquishes the uses of the tuple. */
	bool				retryPossible);
					

/*
 * Indicate we are aborting the create of a relation file.
 *
 * This state will make sure the relation gets dropped after a system crash.
 */
extern PersistentFileSysObjStateChangeResult PersistentRelation_MarkAbortingCreate(
	RelFileNode 		*relFileNode,
				/* The tablespace, database, and relation OIDs for the aborting create. */
	int32				segmentFileNum,
	ItemPointer 		persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the relation. */
	int64				persistentSerialNum,
				/* Serial number for the relation.	Distinquishes the uses of the tuple. */
	bool				retryPossible);

/*
 * Indicate we phsyicalled removed the relation file.
 */
extern void PersistentRelation_Dropped(
	RelFileNode 		*relFileNode,
				/* The tablespace, database, and relation OIDs for the dropped relation. */
	int32				segmentFileNum,
	ItemPointer 		persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the relation. */
	int64				persistentSerialNum);
				/* Serial number for the relation.	Distinquishes the uses of the tuple. */

extern void PersistentRelation_MarkBufPoolRelationForScanIncrementalResync(
	RelFileNode 		*relFileNode,
				/* The tablespace, database, and relation OIDs for the created relation. */
	ItemPointer 		persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the relation. */
	int64				persistentSerialNum);
				/* Serial number for the relation.	Distinquishes the uses of the tuple. */

// -----------------------------------------------------------------------------
// Shmem and Startup/Shutdown
// -----------------------------------------------------------------------------
				
/*
 * Return the required shared-memory size for this module.
 */
extern Size PersistentRelation_ShmemSize(void);
								
/*
 * Initialize the shared-memory for this module.
 */
extern void PersistentRelation_ShmemInit(void);
extern void PersistentRelation_AddMirrorAll(int16 pridbid, int16 mirdbid);
extern void PersistentRelation_RemoveSegment(int16 dbid, bool ismirror);
extern void PersistentRelation_ActivateStandby(int16 newmaster,
											   int16 oldmaster);
 
#endif   /* CDBPERSISTENTRELATION_H */
