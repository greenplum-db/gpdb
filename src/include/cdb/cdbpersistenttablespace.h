/*-------------------------------------------------------------------------
 *
 * cdbpersistenttablespace.h
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbpersistenttablespace.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBPERSISTENTTABLESPACE_H
#define CDBPERSISTENTTABLESPACE_H

#include "access/persistentfilesysobjname.h"
#include "access/xlogmm.h"
#include "cdb/cdbsharedoidsearch.h"
#include "storage/itemptr.h"
#include "cdb/cdbpersistentfilesysobj.h"

extern void PersistentTablespace_Reset(void);

extern PersistentFileSysState PersistentTablespace_GetState(
	Oid 	tablespaceOid);

extern void PersistentTablespace_LookupTidAndSerialNum(
	Oid 		tablespaceOid,
				/* The tablespace OID for the lookup. */

	ItemPointer		persistentTid,
				/* TID of the gp_persistent_tablespace_node tuple for the rel file */

	int64			*persistentSerialNum);


/*
 * The states of a persistent database directory object usablility.
 */
typedef enum PersistentTablespaceGetFilespaces
{
	PersistentTablespaceGetFilespaces_None= 0,
	PersistentTablespaceGetFilespaces_Ok,
	PersistentTablespaceGetFilespaces_TablespaceNotFound,
	PersistentTablespaceGetFilespaces_FilespaceNotFound,
	MaxPersistentTablespaceGetFilespaces /* must always be last */
} PersistentTablespaceGetFilespaces;

typedef struct TablespaceDirEntryKey
{
	Oid	tablespaceOid;
} TablespaceDirEntryKey;

typedef struct TablespaceDirEntryData
{
	TablespaceDirEntryKey	key;

	Oid						filespaceOid;

	PersistentFileSysState	state;
	int64					persistentSerialNum;
	ItemPointerData 		persistentTid;
	
} TablespaceDirEntryData;
typedef TablespaceDirEntryData *TablespaceDirEntry;
extern HTAB *persistentTablespaceSharedHashTable;

extern PersistentTablespaceGetFilespaces PersistentTablespace_TryGetPrimaryAndMirrorFilespaces(
	Oid 		tablespaceOid,
				/* The tablespace OID for the create. */

	char **primaryFilespaceLocation,
				/* The primary filespace directory path.  Return NULL for global and base. */
	
	char **mirrorFilespaceLocation,
				/* The primary filespace directory path.  Return NULL for global and base. 
				 * Or, returns NULL when mirror not configured. */
				 
	Oid *filespaceOid);

extern void PersistentTablespace_GetPrimaryAndMirrorFilespaces(
	Oid 		tablespaceOid,
				/* The tablespace OID for the create. */

	char **primaryFilespaceLocation,
				/* The primary filespace directory path.  Return NULL for global and base. */
	
	char **mirrorFilespaceLocation);
				/* The primary filespace directory path.  Return NULL for global and base. 
				 * Or, returns NULL when mirror not configured. */

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
extern void PersistentTablespace_MarkCreatePending(
	Oid 		filespaceOid,
				/* The filespace where the tablespace lives. */

	Oid 		tablespaceOid,
				/* The tablespace OID for the create. */
				
	MirroredObjectExistenceState mirrorExistenceState,

	ItemPointer		persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */

	int64			*persistentSerialNum,

	bool			flushToXLog);
				/* When true, the XLOG record for this change will be flushed to disk. */

// -----------------------------------------------------------------------------
// Transaction End	
// -----------------------------------------------------------------------------
				
/*
 * Indicate the transaction commited and the relation is officially created.
 */
extern void PersistentTablespace_Created(							
	Oid 		tablespaceOid,
				/* The tablespace OID for the create. */
	
	ItemPointer 	persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */
				
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
extern PersistentFileSysObjStateChangeResult PersistentTablespace_MarkDropPending(
	Oid 		tablespaceOid,
				/* The tablespace OID for the drop. */
	
	ItemPointer 	persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */
							
	int64			persistentSerialNum,
				/* Serial number for the relation.	Distinquishes the uses of the tuple. */

	bool			retryPossible);

/*
 * Indicate we are aborting the create of a relation file.
 *
 * This state will make sure the relation gets dropped after a system crash.
 */
extern PersistentFileSysObjStateChangeResult PersistentTablespace_MarkAbortingCreate(
	Oid 		tablespaceOid,
				/* The tablespace OID for the aborting create. */
							
	ItemPointer 	persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */
							
	int64			persistentSerialNum,
				/* Serial number for the relation.	Distinquishes the uses of the tuple. */

	bool			retryPossible);
					
/*
 * Indicate we phsyicalled removed the relation file.
 */
extern void PersistentTablespace_Dropped(
	Oid 		tablespaceOid,
				/* The tablespace OID for the dropped tablespace. */
										
	ItemPointer 	persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */
							
	int64			persistentSerialNum);
				/* Serial number for the relation.	Distinquishes the uses of the tuple. */
	
// -----------------------------------------------------------------------------
// Re-build tablespace persistent table 'gp_persistent_tablespace_node'
// -----------------------------------------------------------------------------
extern void PersistentTablespace_AddCreated(
											 Oid 		filespaceOid,
											 /* The filespace where the tablespace lives. */
											 
											 Oid 		tablespaceOid,
											 /* The tablespace OID for the create. */
											 
											 MirroredObjectExistenceState mirrorExistenceState,
											 											 
											 bool			flushToXLog);
											 /* When true, the XLOG record for this change will be flushed to disk. */

// -----------------------------------------------------------------------------
// Shmem and Startup/Shutdown
// -----------------------------------------------------------------------------
				
/*
 * Return the required shared-memory size for this module.
 */
extern Size PersistentTablespace_ShmemSize(void);
								
/*
 * Initialize the shared-memory for this module.
 */
extern void PersistentTablespace_ShmemInit(void);

extern void PersistentTablespace_AddMirrorAll(int16 pridbid,
											  int16 mirdbid);
extern void PersistentTablespace_RemoveSegment(int16 dbid, bool ismirror);
extern void PersistentTablespace_ActivateStandby(int16 oldmaster,
												 int16 newmaster);
extern Oid PersistentTablespace_GetFileSpaceOid(Oid tablespaceOid);

#ifdef MASTER_MIRROR_SYNC 
void get_tablespace_data(tspc_agg_state **tas, char *caller);
#endif

#endif   /* CDBPERSISTENTTABLESPACE_H */
