/*-------------------------------------------------------------------------
 *
 * cdbmirroredappendonly.h
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbmirroredappendonly.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBMIRROREDAPPENDONLY_H
#define CDBMIRROREDAPPENDONLY_H

#include "utils/palloc.h"
#include "storage/fd.h"
#include "storage/relfilenode.h"
#include "storage/smgr.h"
#include "cdb/cdbfilerepprimary.h"
#include "cdb/cdbpersistentstore.h"

/*
 * This module is for doing mirrored writes for Append-Only relation files privately being written
 * by a backend process.
 *
 * It is intended for Append-Only relation files not under the management of the Buffer Pool.
 */

/*
 * This structure contains write open information.  Consider the fields
 * inside to be private.
 */
typedef struct MirroredAppendOnlyOpen
{
	bool	isActive;

	RelFileNode	relFileNode;
	
	uint32		segmentFileNum;
	
	char		mirrorFilespaceLocation[MAXPGPATH+1];
					// UNDONE: Consider palloc'ing this instead of statically allocating
					// UNDONE: to use less stack space...
	
	
	File		primaryFile;

	MirrorDataLossTrackingState mirrorDataLossTrackingState;

	int64						mirrorDataLossTrackingSessionNum;

	bool						create;

	bool						primaryOnlyToLetResynchronizeWork;
	
	bool						mirrorOnly;
	
	bool						copyToMirror;

	bool						guardOtherCallsWithMirroredLock;
	
	StorageManagerMirrorMode	mirrorMode;

	bool						mirrorDataLossOccurred;
} MirroredAppendOnlyOpen;

// -----------------------------------------------------------------------------
// Open, Flush, and Close 
// -----------------------------------------------------------------------------

/*
 * We call MirroredAppendOnly_Create with the MirroredLock already held.
 */
extern void MirroredAppendOnly_Create(
	RelFileNode 				*relFileNode,
				/* The tablespace, database, and relation OIDs for the open. */

	int32						segmentFileNum,
				/* Which segment file. */
	
	char						*relationName,
					/* For tracing only.  Can be NULL in some execution paths. */
	
	MirrorDataLossTrackingState mirrorDataLossTrackingState,

	int64						mirrorDataLossTrackingSessionNum,
	
	int 						*primaryError,
	
	bool						*mirrorDataLossOccurred);


extern void MirroredAppendOnly_MirrorReCreate(
	RelFileNode 				*relFileNode,
				/* The tablespace, database, and relation OIDs for the open. */

	int32						segmentFileNum,
				/* Which segment file. */
	
	MirrorDataLossTrackingState mirrorDataLossTrackingState,

	int64						mirrorDataLossTrackingSessionNum,
	
	bool						*mirrorDataLossOccurred);

/*
 * MirroredAppendOnly_OpenReadWrite will acquire and release the MirroredLock.
 */
extern void MirroredAppendOnly_OpenReadWrite(
	MirroredAppendOnlyOpen		*open,
				/* The resulting open struct. */

	RelFileNode 				*relFileNode,
				/* The tablespace, database, and relation OIDs for the open. */

	int32						segmentFileNum,
				/* Which segment file. */
	
	char						*relationName,
					/* For tracing only.  Can be NULL in some execution paths. */
	
	int64						logicalEof,
				/* The logical EOF to begin appending the new data. */

	bool						traceOpenFlags,

	ItemPointer					persistentTid,

	int64						persistentSerialNum,
	
	int 						*primaryError);

extern void MirroredAppendOnly_AddMirrorResyncEofs(
	RelFileNode						*relFileNode,

	int32							segmentFileNum,

	char							*relationName,

	ItemPointer						persistentTid,

	int64							persistentSerialNum,

	MirroredLockLocalVars 			*mirroredLockByRefVars,

	bool							originalMirrorCatchupRequired,

	MirrorDataLossTrackingState 	originalMirrorDataLossTrackingState,

	int64 							originalMirrorDataLossTrackingSessionNum,

	int64							mirrorNewEof);

extern void MirroredAppendOnly_EndXactCatchup(
		int 							entryIndex,
	
		RelFileNode 					*relFileNode,
	
		int32							segmentFileNum,
	
		int 							nestLevel,
	
		char							*relationName,
	
		ItemPointer 					persistentTid,
	
		int64							persistentSerialNum,
	
		MirroredLockLocalVars			*mirroredLockVarsByRef,
	
		bool							lastMirrorCatchupRequired,
	
		MirrorDataLossTrackingState 	lastMirrorDataLossTrackingState,
	
		int64							lastMirrorDataLossTrackingSessionNum,
	
		int64							mirrorNewEof);

extern void MirroredAppendOnly_OpenResynchonize(
	MirroredAppendOnlyOpen		*open,
				/* The resulting open struct. */

	RelFileNode 				*relFileNode,
				/* The tablespace, database, and relation OIDs for the open. */

	int32						segmentFileNum,
				/* Which segment file. */
		
	int64						logicalEof,
				/* The logical EOF to begin appending the new data. */
									
	int 						*primaryError,
	
	bool						*mirrorDataLossOccurred);

/*
 * Flush and Close an Append-Only relation file.
 *
 * If the flush is unable to complete on the mirror, then information (segment file, old EOF, 
 * new EOF) on the new Append-Only data will be added to the commit, distributed commit,
 * distributed prepared and commit prepared XLOG records so that data can be resynchronized
 * later.
 */
extern void MirroredAppendOnly_FlushAndClose(
	MirroredAppendOnlyOpen 		*open,
				/* The open struct. */				

	int 						*primaryError,
	
	bool					*mirrorDataLossOccurred,

	bool					*mirrorCatchupRequired,

	MirrorDataLossTrackingState 	*originalMirrorDataLossTrackingState,
	
	int64 							*originalMirrorDataLossTrackingSessionNum);

/*
 * Flush an Append-Only relation file.
 */
extern void MirroredAppendOnly_Flush(
	MirroredAppendOnlyOpen 		*open,
				/* The open struct. */				

	int 						*primaryError,
	
	bool						*mirrorDataLossOccurred);

/*
 * Close an Append-Only relation file.
 */
extern void MirroredAppendOnly_Close(
	MirroredAppendOnlyOpen 	*open,
				/* The open struct. */				

	bool					*mirrorDataLossOccurred);


extern void MirroredAppendOnly_Drop(
	RelFileNode					*relFileNode,
	 
	int32						segmentFileNum,

	char						*relationName,
					/* For tracing only.  Can be NULL in some execution paths. */
	
	bool  						primaryOnly,

	int							*primaryError,

	bool						*mirrorDataLossOccurred);


extern void MirroredAppendOnly_MirrorReDrop(
	RelFileNode					*relFileNode,
	 
	int32						segmentFileNum,
	
	MirrorDataLossTrackingState mirrorDataLossTrackingState,

	int64						mirrorDataLossTrackingSessionNum,
	
	bool						*mirrorDataLossOccurred);

// -----------------------------------------------------------------------------
// Append 
// -----------------------------------------------------------------------------
				
/*
 * Append mirrored.
 */
extern void MirroredAppendOnly_Append(
	MirroredAppendOnlyOpen *open,
				/* The open struct. */

	void		*appendData,
				/* Pointer to the Append-Only data. */

	int32		appendDataLen,
	
				/* The byte length of the Append-Only data. */
	int 		*primaryError,
	
	bool		*mirrorDataLossOccurred);

// -----------------------------------------------------------------------------
// Truncate
// ----------------------------------------------------------------------------
extern void MirroredAppendOnly_Truncate(
	MirroredAppendOnlyOpen *open,
				/* The open struct. */
	
	int64		position,
				/* The position to cutoff the data. */

	int 		*primaryError,
	
	bool		*mirrorDataLossOccurred);


// -----------------------------------------------------------------------------
// Read local side (primary segment)
// ----------------------------------------------------------------------------
extern int MirroredAppendOnly_Read(
	MirroredAppendOnlyOpen *open,
	/* The open struct. */
	
	void					*buffer,
	/* Pointer to the buffer. */
	
	int32					bufferLen);

#ifdef USE_SEGWALREP
extern void
ao_insert_replay(XLogRecord *record);

extern void
xlog_ao_truncate(MirroredAppendOnlyOpen *open, int64 offset);

extern void
ao_truncate_replay(XLogRecord *record);
#endif

#endif   /* CDBMIRROREDAPPENDONLY_H */
