/*
 *  cdbfilerepresyncmanager.h
 *  
 *
 *  Copyright 2009-2010 Greenplum Inc. All rights reserved.
 *
 */

#ifndef CDBFILEREPRESYNCMANAGER_H
#define CDBFILEREPRESYNCMANAGER_H

#include "cdb/cdbfilerep.h"
#include "access/persistentfilesysobjname.h"
#include "cdb/cdbresynchronizechangetracking.h"

/* 
 * Resync Hash Table located in shared memory keeps
 * track of files that are currently in resync.
 * One entry per file.
 * It has fixed MAX number of files to be resynced
 *
 */
typedef struct FileRepResyncHashEntry_s {
	
	char							fileName[MAXPGPATH+1]; 
	/* 
	 * It identifies mirror relation path.
	 * It is key. It has to be first field in the structure. 
	 */
	
	RelFileNode						relFileNode;
	
	int32							segmentFileNum;
	
	PersistentFileSysRelStorageMgr 	relStorageMgr; 
	/* relationType (Buffer Pool or AppendOnly) */
	
	MirroredRelDataSynchronizationState mirrorDataSynchronizationState;
	/* relationOperation (Full, IncrPage or IncrScan) */

	int64							mirrorBufpoolResyncChangedPageCount;

	XLogRecPtr						mirrorBufpoolResyncCkptLoc;
	/* beginIncrResyncLSN */
	
	BlockNumber 					mirrorBufpoolResyncCkptBlockNum;
	
	int64							mirrorAppendOnlyLossEof;
	
	int64							mirrorAppendOnlyNewEof;
	
	ItemPointerData					persistentTid;
	
	int64							persistentSerialNum;
	
	FileRepResyncState_e			fileRepResyncState;
	
} FileRepResyncHashEntry_s;



extern Size FileRepResync_ShmemSize(void);

extern void FileRepResync_ShmemInit(void);

extern void FileRepResync_Cleanup(void);

extern void FileRepPrimary_StartResyncManager(void);

extern FileRepResyncHashEntry_s* FileRepPrimary_GetResyncEntry(ChangeTrackingRequest **request);

extern int FileRepResync_UpdateEntry(
									 FileRepResyncHashEntry_s*	entry);

extern XLogRecPtr FileRepResync_GetEndIncrResyncLSN(void);

extern XLogRecPtr FileRepResync_GetEndFullResyncLSN(void);

extern void FileRepResyncManager_SetEndResyncLSN(
												 XLogRecPtr endResyncLSN);

extern int FileRepResync_IncAppendOnlyCommitCount(void);

extern int FileRepResync_DecAppendOnlyCommitCount(int	count);

extern int FileRepResync_GetAppendOnlyCommitCount(void);

extern bool FileRepResync_IsReMirrorAllowed(void);

extern void FileRepResync_SetReMirrorAllowed(void);

extern void FileRepResync_ResetReMirrorAllowed(void);

extern int64 FileRepResync_GetBlocksSynchronized(void);

extern int64 FileRepResync_GetTotalBlocksToSynchronize(void);

extern void FileRepResync_SetTotalBlocksToSynchronize(int64 totalBlocksToSynchronize);

extern void FileRepResync_AddToTotalBlocksToSynchronize(int64 moreBlocksToSynchronize);

extern struct timeval FileRepResync_GetEstimateResyncCompletionTime(void);

extern void FileRepResyncManager_ResyncFlatFiles(void);

extern void FileRepResync_DecFsobjCount(void);

extern int FileRepResync_GetCurFsobjCount(void);

extern int FileRepResync_GetTotalFsobjCount(void);

#endif  /* CDBFILEREPRESYNCMANAGER_H */
