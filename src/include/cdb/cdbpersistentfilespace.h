/*-------------------------------------------------------------------------
 *
 * cdbpersistentfilespace.h
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbpersistentfilespace.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBPERSISTENTFILEESPACE_H
#define CDBPERSISTENTFILEESPACE_H

#include "access/persistentfilesysobjname.h"
#include "access/xlogmm.h"
#include "cdb/cdbsharedoidsearch.h"
#include "storage/itemptr.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "cdb/cdbpersistenttablespace.h"

extern void PersistentFilespace_ConvertBlankPaddedLocation(
	char 		**filespaceLocation,

	char 		*locationBlankPadded,

	bool		isPrimary);

extern void PersistentFilespace_LookupTidAndSerialNum(
	Oid 		filespaceOid,
				/* The filespace OID for the lookup. */

	ItemPointer		persistentTid,
				/* TID of the gp_persistent_filespace_node tuple for the rel file */

	int64			*persistentSerialNum);


extern PersistentTablespaceGetFilespaces
PersistentFilespace_GetFilespaceFromTablespace(Oid tablespaceOid,
											   char **primaryFilespaceLocation,
											   char **mirrorFilespaceLocation,
											   Oid *filespaceOid);

extern bool PersistentFilespace_TryGetPrimaryAndMirror(
	Oid 		filespaceOid,
				/* The filespace OID to lookup. */

	char **primaryFilespaceLocation,
				/* The primary filespace directory path.  Return NULL for global and base. */
	
	char **mirrorFilespaceLocation);
				/* The primary filespace directory path.  Return NULL for global and base. 
				 * Or, returns NULL when mirror not configured. */

extern void PersistentFilespace_GetPrimaryAndMirror(
	Oid 		filespaceOid,
				/* The filespace OID to lookup. */

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
extern void PersistentFilespace_MarkCreatePending(
	Oid 		filespaceOid,
				/* The filespace where the filespace lives. */

	int16		primaryDbId,
	
	char		*primaryFilespaceLocation,
				/* 
				 * The primary filespace directory path.  NOT Blank padded.
				 * Just a NULL terminated string.
				 */
	
	int16		mirrorDbId,
	
	char		*mirrorFilespaceLocation,

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
extern void PersistentFilespace_Created(							
	Oid 		filespaceOid,
				/* The filespace OID for the create. */
	
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
extern PersistentFileSysObjStateChangeResult PersistentFilespace_MarkDropPending(
	Oid 		filespaceOid,
				/* The filespace OID for the drop. */
	
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
extern PersistentFileSysObjStateChangeResult PersistentFilespace_MarkAbortingCreate(
	Oid 		filespaceOid,
				/* The filespace OID for the aborting create. */
							
	ItemPointer 	persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */
							
	int64			persistentSerialNum,
				/* Serial number for the relation.	Distinquishes the uses of the tuple. */

	bool			retryPossible);
					
/*
 * Indicate we phsyicalled removed the relation file.
 */
extern void PersistentFilespace_Dropped(
	Oid 		filespaceOid,
				/* The filespace OID for the dropped filespace. */
										
	ItemPointer 	persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */
							
	int64			persistentSerialNum);
				/* Serial number for the relation.	Distinquishes the uses of the tuple. */
	

/* 
 * Identify the mirror dbid referenced in the gp_persistet_filespae_node table 
 */
extern int16 PersistentFilespace_LookupMirrorDbid(int16 primaryDbid);

// -----------------------------------------------------------------------------
// Shmem and Startup/Shutdown
// -----------------------------------------------------------------------------
				
/*
 * Return the required shared-memory size for this module.
 */
extern Size PersistentFilespace_ShmemSize(void);
								
/*
 * Initialize the shared-memory for this module.
 */
extern void PersistentFilespace_ShmemInit(void);

extern void PersistentFilespace_AddMirror(Oid filespace, char *mirpath,
							  			  int16 pridbid, int16 mirdbid,
										  bool set_mirror_existence);
extern void PersistentFilespace_RemoveSegment(int16 dbid, bool ismirror);
extern void PersistentFilespace_ActivateStandby(int16 oldmaster,
												int16 newmaster);
#ifdef MASTER_MIRROR_SYNC
extern void get_filespace_data(fspc_agg_state **fas, char *caller);
#endif

#endif   /* CDBPERSISTENTFILEESPACE_H */
