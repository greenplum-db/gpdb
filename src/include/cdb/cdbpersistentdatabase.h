/*-------------------------------------------------------------------------
 *
 * cdbpersistentdatabase.h
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbpersistentdatabase.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBPERSISTENTDATABASE_H
#define CDBPERSISTENTDATABASE_H

#include "storage/itemptr.h"
#include "cdb/cdbsharedoidsearch.h"
#include "access/persistentfilesysobjname.h"
#include "access/xlogmm.h"
#include "cdb/cdbpersistentfilesysobj.h"

typedef struct DatabaseDirEntryData
{
	SharedOidSearchObjHeader	header;
					/* Search header.
					 *
					 * ****** MUST BE THE FIRST FIELD ******** 
					 *
					 */

	PersistentFileSysState	state;
	int64					persistentSerialNum;
	ItemPointerData 		persistentTid;

	int32	iteratorRefCount;

} DatabaseDirEntryData;
typedef DatabaseDirEntryData *DatabaseDirEntry;



/*
 * This module is for generic relation file create and drop.
 *
 * For create, it makes the file-system create of an empty file fully transactional so
 * the relation file will be deleted even on system crash.  The relation file could be a heap,
 * index, or append-only (row- or column-store).
 */

// -----------------------------------------------------------------------------
// Iterate	
// -----------------------------------------------------------------------------

extern void PersistentDatabase_DirIterateInit(void);

extern bool PersistentDatabase_DirIterateNext(
	DbDirNode				*dbDirNode,
	PersistentFileSysState	*state,
	ItemPointer				persistentTid,
	int64					*persistentSerialNum);

extern void PersistentDatabase_DirIterateClose(void);

extern bool PersistentDatabase_DbDirExistsUnderLock(
	DbDirNode				*dbDirNode);

extern void PersistentDatabase_Reset(void);

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
extern void PersistentDatabase_MarkCreatePending(
	DbDirNode 		*dbDirNode,
				/* The tablespace and database OIDs for the create. */
	MirroredObjectExistenceState mirrorExistenceState,
	ItemPointer		persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */
	int64			*persistentSerialNum,
	bool			flushToXLog);
				/* When true, the XLOG record for this change will be flushed to disk. */

extern void PersistentDatabase_AddCreated(
	DbDirNode		*dbDirNode,
				/* The tablespace and database OIDs for the create. */
	MirroredObjectExistenceState mirrorExistenceState,
	ItemPointer 	persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */
	bool			flushToXLog);
				/* When true, the XLOG record for this change will be flushed to disk. */
					
// -----------------------------------------------------------------------------
// Transaction End	
// -----------------------------------------------------------------------------
				
/*
 * Indicate the transaction commited and the relation is officially created.
 */
extern void PersistentDatabase_Created(
	DbDirNode		*dbDirNode,
				/* The tablespace and database OIDs for the created relation. */
	ItemPointer 	persistentTid,
				/* TID of the gp_persistent_database_node tuple for the rel file */
	int64			persistentSerialNum,
				/* Serial number for the relation.	Distinquishes the uses of the tuple. */
	bool			retryPossible);
					
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
extern PersistentFileSysObjStateChangeResult PersistentDatabase_MarkDropPending(
	DbDirNode		*dbDirNode,
				/* The tablespace and database OIDs for the drop. */
				
	ItemPointer 	persistentTid,
				/* TID of the gp_persistent_database_node tuple for the rel file */
							
	int64			persistentSerialNum,
				/* Serial number for the relation.	Distinquishes the uses of the tuple. */

	bool			retryPossible);

/*
 * Indicate we are aborting the create of a relation file.
 *
 * This state will make sure the relation gets dropped after a system crash.
 */
extern PersistentFileSysObjStateChangeResult PersistentDatabase_MarkAbortingCreate(
	DbDirNode		*dbDirNode,
				/* The tablespace and database OIDs for the aborting create. */
							
	ItemPointer 	persistentTid,
				/* TID of the gp_persistent_database_node tuple for the rel file */
							
	int64			persistentSerialNum,
				/* Serial number for the relation.	Distinquishes the uses of the tuple. */

	bool			retryPossible);
					
/*
 * Indicate we phsyicalled removed the relation file.
 */
extern void PersistentDatabase_Dropped(
	DbDirNode		*dbDirNode,
				/* The tablespace and database OIDs for the dropped relation. */
				
	ItemPointer 	persistentTid,
				/* TID of the gp_persistent_database_node tuple for the rel file */
							
	int64			persistentSerialNum);
				/* Serial number for the relation.	Distinquishes the uses of the tuple. */

extern bool PersistentDatabase_DirIsCreated(
	DbDirNode		*dbDirNode);
	
extern void PersistentDatabase_MarkJustInTimeCreatePending(
	DbDirNode 		*dbDirNode,

	MirroredObjectExistenceState 	mirrorExistenceState,

	ItemPointer 	persistentTid,
				/* TID of the gp_persistent_database_node tuple for the rel file */

	int64			*persistentSerialNum);
				/* Serial number for the relation.	Distinquishes the uses of the tuple. */

extern void PersistentDatabase_JustInTimeCreated(
	DbDirNode 		*dbDirNode,

	ItemPointer 	persistentTid,
				/* TID of the gp_persistent_database_node tuple for the rel file */

	int64			persistentSerialNum);
				/* Serial number for the relation.	Distinquishes the uses of the tuple. */
	
extern void PersistentDatabase_AbandonJustInTimeCreatePending(
	DbDirNode 		*dbDirNode,

	ItemPointer 	persistentTid,
				/* TID of the gp_persistent_database_node tuple for the rel file */

	int64			persistentSerialNum);
				/* Serial number for the relation.	Distinquishes the uses of the tuple. */
	
// -----------------------------------------------------------------------------
// Shmem and Startup/Shutdown
// -----------------------------------------------------------------------------
				
/*
 * Return the required shared-memory size for this module.
 */
extern Size PersistentDatabase_ShmemSize(void);
								
/*
 * Initialize the shared-memory for this module.
 */
extern void PersistentDatabase_ShmemInit(void);
extern void PersistentDatabase_AddMirrorAll(int16 pridbid, int16 mirdbid);
extern void PersistentDatabase_RemoveSegment(int16 dbid, bool ismirror);
extern void PersistentDatabase_ActivateStandby(int16 newmaster,
											   int16 oldmaster);
extern void xlog_create_database(DbDirNode *db);

extern void get_database_data(dbdir_agg_state **das, char *caller);

#endif   /* CDBPERSISTENTDATABASE_H */

