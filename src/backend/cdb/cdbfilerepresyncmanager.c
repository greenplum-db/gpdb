/*-------------------------------------------------------------------------
 *
 * cdbfilerepresyncmanager.c
 *
 * Portions Copyright (c) 2009-2010 Greenplum Inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbfilerepresyncmanager.c
 *
 *-------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
 * INVARIANCES
 *
 *		*) Change Tracking
 *				*) Change Tracking is turned ON when mirror segment is down
 *				   (gp_segment_configuration is in CHANGE TRACKING after failover)
 *				*) In the transition to Change Tracking recovery from the latest checkpoint is done
 *				*) Change tracking keeps track of changes to Heap Data Store
 *				   (RelFileNode, Block#, LSN, Persistent TID, Persistent Serial Number)
 *				*) Change Tracking is turned OFF (disabled) during resynchronization transition
 *				   while primary segment is suspended
 *					*) That means that new changes are not tracked and
 *					   Change tracking log files are NOT deleted
 *				*) Change Tracking log files are deleted when primary and mirror segment
 *				   transitions to InSYNC state
 *
 *		*) Resynchronization (full copy, incremental file copy, incremental block copy)
 *				*) Take last LSN to be resynced “last resync LSN”
 *				*) Resync to mirror all CREATE file operations (RelFileNode, segmentFileNum)
 *				*) Resync to mirror all DROP file operations (RelFileNode, segmentFileNum)
 *				*) Resync to mirror all file changes up to “last resync LSN”.
 *				   Three options available:
 *						*) Full Copy
 *						   For each segment file in a relation for each relation in a
 *						   segment repeat the following steps:
 *								*) Copy all blocks in segment file that has LSN < “last resync LSN”
 *								*) Blocks that has LSN =>”last resync LSN” will be mirrored
 *								*) After copy of each segment file take “resync CKPT”
 *								   (persistently store the latest LSN, it identifies the start
 *									resync point in case of restarting resync due to failures)
 *						*) Incremental File Copy
 *						   For each segment file in a relation that was modified during change tracking
 *								*) Copy all blocks in segment file that has LSN < “last resync LSN”
 *								*) Blocks that has LSN =>”last resync LSN” will be mirrored
 *								*) After copy of each segment file take “resync CKPT”
 *								   (persistently store the latest LSN)
 *						*) Incremental Block Copy
 *						   For each segment file in a relation that was modified during change tracking
 *								*) Copy all blocks in segment file that has LSN < “last resync LSN”
 *								*) Blocks that has LSN =>”last resync LSN” will be mirrored
 *								*) After copy of each segment file take “resync CKPT”
 *								   (persistently store the latest LSN)
 *				*) Crash Recovery of primary segment during resynchronization will
 *				   restart copy from “resync CKPT”
 *				*) Mirror down during resynchronization will turn Change tracking ON and
 *				   turn Resynchronization OFF
 *				*) When Mirror comes up and resynchronization is restarted
 *				   the resynchronization will be restarted from the “resync CKPT”
 *				*) Changes to be resynchronized are cumulative when Change Tracking is
 *				   turned on multiple times
 *				*) Catch up of pg_twophase, pg_xlog, pg_control, ... before transition to InSYNC mode
 *
 *		*) Ongoing IO to primary segment
 *				*) When resynchronization transition from its recovery to ready state
 *				   changes to primary segment are mirrored (last resync LSN + 1)
 *				*) If primary segment crashes during resynchronization then during
 *				   recovery any change applied during WAL replay on primary is applied
 *				   also to mirror
 *				*) If mirror segment goes down then Change Tracking is turned ON on the
 *				   primary. No disruption to IO on primary segment.
 *				*) During resynchronization WAL and flat files are excluded from mirroring.
 *------------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
 *	The following steps are performed during dataState == DataStateResynchronizing
 *		*) FULL Copy
 *			*) Transition Segment State
 *				*) mark all relations re-create needed
 *				*) mark all relations full copy request
 *
 *
 *			*) Ready Segment State
 *
 *		*) INCREMENTAL Copy
 *				*) Transition Segment State
 *
 *				*) Ready Segment State
 *					*) Scan Change Tracking to get all relations to be marked
 *					   in persistent file system object
 *------------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
 Transition from Change Tracking to InResync on primary segment
 ==============================================================
 1)	Shutdown FileRep Main process
 2)	postmaster set
		*) dataState = DataStateInResync
		*) segmentState = SegmentStateInResyncTransition
 3)	Start FileRep Main process
 4)	Start 1 FileRep backend process as a Resync Manager
		*) FileRepMain -> FileRepPrimary_StartResyncManager()
 5)	Start N FileRep backend processes as a Resync Worker  ( N should be GUC, Min=1, Max=8, Default =1 (Default will be increased later))
		*) FileRepMain -> FileRepPrimary_StartResyncWorker()
 6)	FileRep Resync backend processes exit when resynchronization is completed
	ResyncManager set
		*) dataState == DataStateInSync
		*) segmentState = SegmentStateReady

 Crash Recovery when dataState = DataStateInResync on primary segment
 ====================================================================
 1) postmaster sets
		*) dataState = DataStateInResync
		*) segmentState = SegmentStateInitialization
 2)	Start FileRep Main process
 3)	Start 1 FileRep backend process as a Resync Manager
		*)	FileRepMain() -> FileRepPrimary_StartResyncManager()
 5)	Start N FileRep backend processes as a Resync Worker  ( N should be GUC, Min=1, Max=8, Default =1 (Default will be increased later))
		*) FileRepMain() -> FileRepPrimary_StartResyncWorker()
 6)	Resync Manager process checks the state of resync transition
	If resync transition has not been completed yet then
		a) Resync Manager sets
			*) dataState = DataStateInChangeTracking
			*) segmentState = SegmentStateReady or SegmentStateChangeTrackingDisabled
		b) postmaster run XLOG recovery
		c) postmaster sets
			*) dataState = DataStateInResync
			*) segmentState = SegmentStateInResyncTransition
	else
		a) Resync Manager sets
			*) dataState = DataStateInResync
			*) segmentState = SegmentStateReady
		b) postmaster run XLOG recovery
			*) dataState = DataStateInResync
			*) segmentState = SegmentStateReady
 6)	FileRep Resync backend processes exit when resynchronization is completed
	ResyncManager set
		*) dataState == DataStateInSync
		*) segmentState = SegmentStateReady
------------------------------------------------------------------------------
*/

#include "postgres.h"

#include <signal.h>

#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "access/twophase.h"
#include "access/slru.h"
#include "cdb/cdbfilerepprimaryrecovery.h"
#include "cdb/cdbfilerepservice.h"
#include "cdb/cdbfilerepprimaryack.h"
#include "cdb/cdbfilerep.h"
#include "cdb/cdbfilerepprimary.h"
#include "cdb/cdbfilerepresyncmanager.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "cdb/cdbmirroredflatfile.h"
#include "cdb/cdbresynchronizechangetracking.h"
#include "postmaster/bgwriter.h"
#include "postmaster/primary_mirror_mode.h"
#include "storage/lmgr.h"
#include "utils/flatfiles.h"
#include "utils/faultinjector.h"


typedef struct FileRepResyncShmem_s
{

	volatile int appendOnlyCommitCount;
	/* This counter is protected by FileRepAppendOnlyCommitCountLock */

	bool		reMirrorAllowed;

	/*
	 * This flag is used just to enable FILEREP proces to perform some
	 * operation (like physical file drops/creates) on Mirror while
	 * InResyncTranstion phase. The flag set/unset has no use/impact for
	 * non-filerep processes.
	 */

	int64		blocksSynchronized;
	/* required to report number of blocks resynchronized */

	int64		totalBlocksToSynchronize;
	/* required to report total number of blocks to be resynchronized */

	struct timeval startResyncTime;

	/*
	 * resynchronization start time, required to report estimate time for
	 * resync to complete
	 */

	slock_t		lock;

	XLogRecPtr	endFullResyncLSN;

	XLogRecPtr	endIncrResyncLSN;

	int			writeCount;
	/* number of relations waiting to be resynchronized */

	int			curFsobjCount;
	/* Current number of file system objects to create/drop during resync */

	int			totalFsobjCount;
	/* Total number of file system objects to create/drop during resync */

	int			resyncInProgressCount;
	/* number of relations in resynchronization */

	int			resyncCompletedCount;

	HTAB	   *fileRepResyncHash;
	/* List of relations to be resynced */

} FileRepResyncShmem_s;

static volatile FileRepResyncShmem_s *fileRepResyncShmem = NULL;

static void FileRepResync_ShmemReInit(void);

static int	FileRepResyncManager_InResyncTransition(void);
static int	FileRepResyncManager_InSyncTransition(void);
static int	FileRepPrimary_RunResyncManager(void);

static void FileRepResync_LockAcquire(void);
static void FileRepResync_LockRelease(void);

static int FileRepResync_InsertEntry(
						  FileRepResyncHashEntry_s *entry);
static FileRepResyncHashEntry_s *FileRepResync_LookupEntry(
						  FileName fileName);
static bool FileRepResync_RemoveEntry(
						  FileName fileName);

static int	FileRepResync_CheckProgress(void);

static void
FileRepResync_LockAcquire(void)
{
	SpinLockAcquire(&fileRepResyncShmem->lock);
}

static void
FileRepResync_LockRelease(void)
{
	SpinLockRelease(&fileRepResyncShmem->lock);
}

void
FileRepResync_DecFsobjCount(void)
{
	Assert(fileRepResyncShmem->curFsobjCount > 0);
	if (fileRepResyncShmem != NULL && (fileRepResyncShmem->curFsobjCount > 0))
	{
		--(fileRepResyncShmem->curFsobjCount);
	}
}

int
FileRepResync_GetCurFsobjCount(void)
{
	return (fileRepResyncShmem != NULL) ?
		fileRepResyncShmem->curFsobjCount : -1;
}

int
FileRepResync_GetTotalFsobjCount(void)
{
	return (fileRepResyncShmem != NULL) ?
		fileRepResyncShmem->totalFsobjCount : -1;
}

int
FileRepResync_IncAppendOnlyCommitCount(void)
{
	/* This counter is protected by FileRepAppendOnlyCommitCountLock; */
	if (fileRepResyncShmem != NULL)
	{
		return ++(fileRepResyncShmem->appendOnlyCommitCount);
	}
	else
	{
		return 0;
	}
}

int
FileRepResync_DecAppendOnlyCommitCount(int count)
{
	/* This counter is protected by FileRepAppendOnlyCommitCountLock; */
	if (fileRepResyncShmem != NULL)
	{
		fileRepResyncShmem->appendOnlyCommitCount -= count;

		Assert(fileRepResyncShmem->appendOnlyCommitCount >= 0);

		return fileRepResyncShmem->appendOnlyCommitCount;
	}
	else
	{
		return 0;
	}
}

int
FileRepResync_GetAppendOnlyCommitCount(void)
{
	/* This counter is protected by FileRepAppendOnlyCommitCountLock; */
	if (fileRepResyncShmem != NULL)
	{
		return fileRepResyncShmem->appendOnlyCommitCount;
	}
	else
	{
		return 0;
	}
}

XLogRecPtr
FileRepResync_GetEndIncrResyncLSN(void)
{
	return fileRepResyncShmem->endIncrResyncLSN;
}

XLogRecPtr
FileRepResync_GetEndFullResyncLSN(void)
{
	return fileRepResyncShmem->endFullResyncLSN;
}

void
FileRepResyncManager_SetEndResyncLSN(XLogRecPtr endResyncLSN)
{
	char		tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];

	Assert(!(endResyncLSN.xlogid == 0 && endResyncLSN.xrecoff == 0));

	if (isFullResync())
	{
		fileRepResyncShmem->endFullResyncLSN.xlogid = endResyncLSN.xlogid;
		fileRepResyncShmem->endFullResyncLSN.xrecoff = endResyncLSN.xrecoff;
	}
	else
	{
		fileRepResyncShmem->endIncrResyncLSN.xlogid = endResyncLSN.xlogid;
		fileRepResyncShmem->endIncrResyncLSN.xrecoff = endResyncLSN.xrecoff;
	}


	snprintf(tmpBuf, sizeof(tmpBuf), "full resync '%s' resync lsn '%s(%u/%u)' ",
			 (isFullResync() == TRUE) ? "true" : "false",
			 XLogLocationToString(&endResyncLSN),
			 endResyncLSN.xlogid,
			 endResyncLSN.xrecoff);

	FileRep_InsertConfigLogEntry(tmpBuf);
}

void
FileRepResync_ResetReMirrorAllowed(void)
{
	fileRepResyncShmem->reMirrorAllowed = FALSE;
}

void
FileRepResync_SetReMirrorAllowed(void)
{
	fileRepResyncShmem->reMirrorAllowed = TRUE;
}

bool
FileRepResync_IsReMirrorAllowed(void)
{
	return ((fileRepResyncShmem != NULL) ?
			(fileRepResyncShmem->reMirrorAllowed == TRUE &&
			 fileRepProcessType == FileRepProcessTypeResyncManager) : false);
}

int64
FileRepResync_GetBlocksSynchronized(void)
{
	return ((fileRepResyncShmem != NULL) ?
			(fileRepResyncShmem->blocksSynchronized) : 0);
}

int64
FileRepResync_GetTotalBlocksToSynchronize(void)
{
	return ((fileRepResyncShmem != NULL) ?
			(fileRepResyncShmem->totalBlocksToSynchronize) : 0);
}

void
FileRepResync_SetTotalBlocksToSynchronize(int64 totalBlocksToSynchronize)
{
	fileRepResyncShmem->totalBlocksToSynchronize = totalBlocksToSynchronize;
}

void
FileRepResync_AddToTotalBlocksToSynchronize(int64 moreBlocksToSynchronize)
{
	fileRepResyncShmem->totalBlocksToSynchronize += moreBlocksToSynchronize;
}

struct timeval
FileRepResync_GetEstimateResyncCompletionTime(void)
{
	char		temp[128];
	pg_time_t	tt;
	struct timeval currentResyncTime;
	struct timeval estimateResyncCompletionTime = {0, 0};

	/*
	 * pull values out of shared memory into local variables so we have
	 * consistent values for calculation here
	 */
	int64		totalBlocksToSynchronize = fileRepResyncShmem == NULL ? 0L : fileRepResyncShmem->totalBlocksToSynchronize;
	int64		blocksSynchronized = fileRepResyncShmem == NULL ? 0L : fileRepResyncShmem->blocksSynchronized;

	if (totalBlocksToSynchronize == 0L || blocksSynchronized == 0L)
	{
		return estimateResyncCompletionTime;
	}
	struct timeval startResyncTime = fileRepResyncShmem->startResyncTime;

	gettimeofday(&currentResyncTime, NULL);

	if (totalBlocksToSynchronize > blocksSynchronized)
	{
		estimateResyncCompletionTime.tv_sec =
			(((currentResyncTime.tv_sec - startResyncTime.tv_sec) *
			  (totalBlocksToSynchronize - blocksSynchronized)) /
			 blocksSynchronized) + currentResyncTime.tv_sec;
	}
	else
	{
		estimateResyncCompletionTime.tv_sec = 0;
		estimateResyncCompletionTime.tv_usec = 0;
	}

	if (Debug_filerep_print)
	{
		tt = (pg_time_t) estimateResyncCompletionTime.tv_sec;
		pg_strftime(temp, sizeof(temp), "%a %b %d %H:%M:%S.%%06d %Y %Z",
					pg_localtime(&tt, session_timezone));

		elog(LOG,
			 "resynchronization info: "
			 "total blocks to synchronize " INT64_FORMAT " "
			 "blocks synchronized " INT64_FORMAT " "
			 "estimate resync completion time '%s' ",
			 totalBlocksToSynchronize,
			 blocksSynchronized,
			 temp);
	}

	return estimateResyncCompletionTime;
}

/****************************************************************
 * FILEREP_RESYNC SHARED MEMORY
 ****************************************************************/

Size
FileRepResync_ShmemSize(void)
{
	Size		size;

	size = hash_estimate_size(
							  (Size) FILEREP_MAX_RESYNC_FILES,
							  sizeof(FileRepResyncHashEntry_s));

	size = add_size(size, sizeof(FileRepResyncShmem_s));

	return size;
}

/*
 * Hash table contains
 * FileName(identifier) is the key in the hash table.
 * Hash table in shared memory is initialized only on primary segment.
 * It is not initialized on mirror and master host.
 */
void
FileRepResync_ShmemInit(void)
{
	HASHCTL		hash_ctl;
	bool		foundPtr;
	struct timeval resyncTime;


	fileRepResyncShmem = (FileRepResyncShmem_s *) ShmemInitStruct("filerep resync",
																  sizeof(FileRepResyncShmem_s),
																  &foundPtr);

	if (fileRepResyncShmem == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 (errmsg("not enough shared memory to run resynchronization"))));
	}

	if (!foundPtr)
	{
		MemSet(fileRepResyncShmem, 0, sizeof(FileRepResyncShmem_s));
	}

	fileRepResyncShmem->appendOnlyCommitCount = 0;

	fileRepResyncShmem->reMirrorAllowed = FALSE;

	fileRepResyncShmem->totalBlocksToSynchronize = 0;

	fileRepResyncShmem->blocksSynchronized = 0;

	gettimeofday(&resyncTime, NULL);

	fileRepResyncShmem->startResyncTime.tv_sec = resyncTime.tv_sec;
	fileRepResyncShmem->startResyncTime.tv_usec = resyncTime.tv_usec;

	SpinLockInit(&fileRepResyncShmem->lock);

	fileRepResyncShmem->endFullResyncLSN.xlogid = 0;
	fileRepResyncShmem->endFullResyncLSN.xrecoff = 0;

	fileRepResyncShmem->endIncrResyncLSN.xlogid = 0;
	fileRepResyncShmem->endIncrResyncLSN.xrecoff = 0;

	fileRepResyncShmem->writeCount = 0;

	fileRepResyncShmem->curFsobjCount = 0;
	fileRepResyncShmem->totalFsobjCount = 0;

	fileRepResyncShmem->resyncInProgressCount = 0;
	fileRepResyncShmem->resyncCompletedCount = 0;

	MemSet(&hash_ctl, 0, sizeof(hash_ctl));
	hash_ctl.keysize = MAXPGPATH;
	hash_ctl.entrysize = sizeof(FileRepResyncHashEntry_s);
	hash_ctl.hash = string_hash;

	fileRepResyncShmem->fileRepResyncHash = ShmemInitHash("filerep resync hash",
														  FILEREP_MAX_RESYNC_FILES,
														  FILEREP_MAX_RESYNC_FILES,
														  &hash_ctl,
														  HASH_ELEM | HASH_FUNCTION);

	if (fileRepResyncShmem->fileRepResyncHash == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 (errmsg("not enough shared memory to run resynchronization"))));
	}

	return;
}

/*
 *
 */
static void
FileRepResync_ShmemReInit(void)
{
	HASH_SEQ_STATUS hash_status;
	FileRepResyncHashEntry_s *entry;
	struct timeval resyncTime;


	if (fileRepResyncShmem == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 (errmsg("not enough shared memory to run resynchronization"))));
	}

	/*
	 * NOTE: Do not zero Commit Work Intent count for ReInit.
	 */
	fileRepResyncShmem->reMirrorAllowed = FALSE;

	fileRepResyncShmem->totalBlocksToSynchronize = 0;

	fileRepResyncShmem->blocksSynchronized = 0;

	gettimeofday(&resyncTime, NULL);

	fileRepResyncShmem->startResyncTime.tv_sec = resyncTime.tv_sec;
	fileRepResyncShmem->startResyncTime.tv_usec = resyncTime.tv_usec;

	SpinLockInit(&fileRepResyncShmem->lock);

	fileRepResyncShmem->endFullResyncLSN.xlogid = 0;
	fileRepResyncShmem->endFullResyncLSN.xrecoff = 0;

	fileRepResyncShmem->endIncrResyncLSN.xlogid = 0;
	fileRepResyncShmem->endIncrResyncLSN.xrecoff = 0;

	fileRepResyncShmem->writeCount = 0;

	fileRepResyncShmem->curFsobjCount = 0;
	fileRepResyncShmem->totalFsobjCount = 0;

	fileRepResyncShmem->resyncInProgressCount = 0;
	fileRepResyncShmem->resyncCompletedCount = 0;

	if (fileRepResyncShmem->fileRepResyncHash == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 (errmsg("not enough shared memory to run resynchronization"))));
	}

	FileRepResync_LockAcquire();

	hash_seq_init(&hash_status, fileRepResyncShmem->fileRepResyncHash);

	while ((entry = (FileRepResyncHashEntry_s *) hash_seq_search(&hash_status)) != NULL)
	{

		FileRepResync_RemoveEntry(entry->fileName);
	}

	FileRepResync_LockRelease();

	return;
}

/*
 *
 */
void
FileRepResync_Cleanup(void)
{
	HASH_SEQ_STATUS hash_status;
	FileRepResyncHashEntry_s *entry;

	if (fileRepResyncShmem == NULL)
	{
		return;
	}

	if (fileRepResyncShmem->fileRepResyncHash == NULL)
	{
		return;
	}

	FileRepResync_LockAcquire();

	hash_seq_init(&hash_status, fileRepResyncShmem->fileRepResyncHash);

	while ((entry = (FileRepResyncHashEntry_s *) hash_seq_search(&hash_status)) != NULL)
	{

		FileRepResync_RemoveEntry(entry->fileName);
		UnlockRelationForResynchronize(
									   &entry->relFileNode,
									   AccessExclusiveLock);
	}

	LockReleaseAll(DEFAULT_LOCKMETHOD, false);

	FileRepResync_LockRelease();

	return;
}

/****************************************************************
 * FILEREP SUB-PROCESS (FileRep Primary RECOVERY Process)
 ****************************************************************/
/*
 *
 * FileRepPrimary_StartResyncManager()
 *
 *
 */
void
FileRepPrimary_StartResyncManager(void)
{
	int			status = STATUS_OK;
	bool		isLastLocTracked = FALSE;

	FileRep_InsertConfigLogEntry("start resync manager");

	Insist(fileRepRole == FileRepPrimaryRole);

	Insist(dataState == DataStateInResync);

	FileRepResync_ShmemReInit();

	while (1)
	{

		if (status != STATUS_OK)
		{
			FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
			FileRepSubProcess_SetState(FileRepStateFault);
		}

		while (FileRepSubProcess_GetState() == FileRepStateFault ||

			   (fileRepShmemArray[0]->state == FileRepStateNotInitialized &&
				FileRepSubProcess_GetState() != FileRepStateShutdownBackends &&
				FileRepSubProcess_GetState() != FileRepStateShutdown))
		{

			FileRepSubProcess_ProcessSignals();
			pg_usleep(50000L);	/* 50 ms */
		}

		if (FileRepSubProcess_GetState() == FileRepStateShutdown ||
			FileRepSubProcess_GetState() == FileRepStateShutdownBackends)
		{

			break;
		}

		if (segmentState == SegmentStateInitialization)
		{

			if (ChangeTracking_RetrieveLastChangeTrackedLoc())
			{
				isLastLocTracked = TRUE;
				FileRepSubProcess_SetState(FileRepStateReady);
				getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);

				while (FileRepSubProcess_GetState() != FileRepStateShutdown &&
					   FileRepSubProcess_GetState() != FileRepStateShutdownBackends &&
					   isDatabaseRunning() == FALSE)
				{

					FileRepSubProcess_ProcessSignals();
					pg_usleep(50000L);	/* 50 ms */
				}

				if (FileRepSubProcess_GetState() == FileRepStateShutdown ||
					FileRepSubProcess_GetState() == FileRepStateShutdownBackends)
				{

					break;
				}

			}
			else
			{

				FileRep_SetDataState(DataStateInChangeTracking, FALSE /* signal postmaster */ );

				if (isFullResync())
				{
					FileRep_SetSegmentState(SegmentStateChangeTrackingDisabled, FaultTypeNotInitialized);
				}
				else
				{
					ChangeTracking_CreateInitialFromPreviousCheckpoint(
																	    /* lastChangeTrackingEndLoc */ NULL);
					FileRepSubProcess_SetState(FileRepStateReady);
				}

				/* Wait that XLOG replay is done */
				while (FileRepSubProcess_GetState() != FileRepStateShutdown &&
					   FileRepSubProcess_GetState() != FileRepStateShutdownBackends &&
					   !(segmentState == SegmentStateInResyncTransition &&
						 dataState == DataStateInResync))
				{

					FileRepSubProcess_ProcessSignals();
					pg_usleep(50000L);	/* 50 ms */
				}

				if (FileRepSubProcess_GetState() == FileRepStateShutdown ||
					FileRepSubProcess_GetState() == FileRepStateShutdownBackends)
				{

					break;
				}
			}

			/*
			 * Database was started. The local copies of ThisTimeLineID and
			 * RedoRecPtr has to be initialized.
			 */
			InitXLOGAccess();
		}

		Insist(segmentState == SegmentStateInResyncTransition ||
			   segmentState == SegmentStateReady);

		FileRepSubProcess_InitHeapAccess();

		if (isLastLocTracked == FALSE)
		{
			status = FileRepResyncManager_InResyncTransition();
		}

		if (status != STATUS_OK ||
			!(FileRepSubProcess_GetState() == FileRepStateReady &&
			  dataState == DataStateInResync))
		{
			continue;
		}

		status = FileRepPrimary_RunResyncManager();

		if (status != STATUS_OK ||
			!(FileRepSubProcess_GetState() == FileRepStateReady &&
			  dataState == DataStateInResync))
		{
			continue;
		}

		status = FileRepResyncManager_InSyncTransition();

		if (status != STATUS_OK)
		{
			continue;
		}

		ereport(LOG,
				(errmsg("mirror transition to sync completed, "
						"primary address(port) '%s(%d)' mirror address(port) '%s(%d)' ",
						fileRepPrimaryHostAddress,
						fileRepPrimaryPort,
						fileRepMirrorHostAddress,
						fileRepMirrorPort),
				 FileRep_errcontext()));
		break;

	} //while (1)

		FileRep_InsertConfigLogEntry("resync manager completed");

}

/*
 * FileRepResyncManager_InResyncTransition()
 */
static int
FileRepResyncManager_InResyncTransition(void)
{
	int			status = STATUS_OK;

	FileRep_InsertConfigLogEntry("run resync transition");

	if (LWLockHeldByMe(MirroredLock))
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 (errmsg("'MirroredLock' is already held by primary resync manager process"))));

	LWLockAcquire(MirroredLock, LW_EXCLUSIVE);

	/*
	 * database transitions to suspended state, IO activity on the segment is
	 * suspended
	 */
	primaryMirrorSetIOSuspended(TRUE);

	FileRep_InsertConfigLogEntry("run resync transition, record last lsn in change tracking");

	ChangeTracking_RecordLastChangeTrackedLoc();

	if (FileRepSubProcess_GetState() == FileRepStateFault)
	{
		goto exit;
	}

	FileRepSubProcess_ProcessSignals();
	if (!((segmentState == SegmentStateInResyncTransition ||
		   segmentState == SegmentStateReady) &&
		  dataState == DataStateInResync))
	{
		goto exit;
	}

	if (isFullResync())
	{
		FileRep_InsertConfigLogEntry("run resync transition, mark full copy");

		PersistentFileSysObj_MarkWholeMirrorFullCopy();
	}
	else
	{
		/*
		 * First, mark the special persistent tables and others as 'Scan
		 * Incremental'.
		 *
		 * These include OIDs {5090, 5091, 5092, 5093, and 5096} and others
		 * (see GpPersistent_SkipXLogInfo).
		 *
		 * And, mark any Buffer Pool managed relations that were physically
		 * truncated as as 'Scan incremental' because we don't know how to
		 * process old changes in the change tracking log.
		 */
		FileRep_InsertConfigLogEntry("run resync transition, mark scan incremental");

		PersistentFileSysObj_MarkSpecialScanIncremental();

		/*
		 * Second, now mark any relations that have changes in the change
		 * tracking log as 'Page Incremental', except those relations marked
		 * 'Scan Incremental' in the first step.
		 */
		FileRep_InsertConfigLogEntry("run resync transition, mark page incremental");

		PersistentFileSysObj_MarkPageIncrementalFromChangeLog();
		if (segmentState == SegmentStateChangeTrackingDisabled)
		{
			status = STATUS_ERROR;
			goto exit;
		}

		/*
		 * Finally, find any Append-Only tables that have mirror data loss.
		 */
		FileRep_InsertConfigLogEntry("run resync transition, mark append only incremental");

		PersistentFileSysObj_MarkAppendOnlyCatchup();
	}

	FileRepSubProcess_ProcessSignals();
	if (!((segmentState == SegmentStateInResyncTransition ||
		   segmentState == SegmentStateReady) &&
		  dataState == DataStateInResync))
	{
		goto exit;
	}

	/*
	 * It is important to keep order of dropping objects before creating
	 * objects since it can happen that persistent table has two entries for
	 * the same object. For e.g Consider that an append only segment file was
	 * created (when mirror was active) and dropped (after mirror went down)
	 * since transaction was aborted. Later, if the same Append Only segment
	 * file was created and transaction was committed (with the Mirror still
	 * down). This situation can cause to have duplicate entries for the same
	 * append only segment file. Now during resync phase if the file is
	 * created on the mirror first (re-create phase) and later dropped (redrop
	 * phase for original file) you can loose the file created by re-created
	 * phase.
	 */
	FileRep_InsertConfigLogEntry("run resync manager, mirror redrop");

	fileRepResyncShmem->totalFsobjCount = fileRepResyncShmem->curFsobjCount
		= PersistentFileSysObj_CountResyncObjects();

	/*
	 * Drop the objects on the mirror that were dropped on the primary when
	 * the mirror was down/inactive by looking at Persistent Tables (all
	 * object types). But note that the accounting information in Persistent
	 * Tables will be updated later. This is because at this stage the segment
	 * is still in resync transition state and not ready. As a result any
	 * operations for e.g. write() still assume that the mirror is
	 * inactive/down and so won't be mirrored. Once the segment changes its
	 * state to ready, all such operations will be correctly mirrored.
	 */
	PersistentFileSysObj_MirrorReDrop();

	FileRep_InsertConfigLogEntry("run resync manager, mirror redrop completed");

	FileRepSubProcess_ProcessSignals();
	if (!((segmentState == SegmentStateInResyncTransition ||
		   segmentState == SegmentStateReady) &&
		  dataState == DataStateInResync))
	{
		goto exit;
	}

	SIMPLE_FAULT_INJECTOR(FileRepTransitionToInResyncMirrorReCreate);

	FileRep_InsertConfigLogEntry("run resync transition, mirror recreate");

	PersistentFileSysObj_MirrorReCreate();

	FileRepSubProcess_ProcessSignals();
	if (!((segmentState == SegmentStateInResyncTransition ||
		   segmentState == SegmentStateReady) &&
		  dataState == DataStateInResync))
	{
		goto exit;
	}

	FileRepSubProcess_SetState(FileRepStateReady);

	SIMPLE_FAULT_INJECTOR(FileRepTransitionToInResyncMarkReCreated);

	/*
	 * Mark Persistent Table entries as 'Dropped (i.e. Free)' to indicate the
	 * drops performed in PersistenFileSysObj_MirrorReDrop(). This module only
	 * updates drop pending/aborting create entries to 'free' in Persistent
	 * tables.
	 */
	FileRep_InsertConfigLogEntry("run resync transition, mark mirror redropped");
	PersistentFileSysObj_MarkMirrorReDropped();

	FileRepSubProcess_ProcessSignals();
	if (!(segmentState == SegmentStateReady &&
		  dataState == DataStateInResync))
	{
		goto exit;
	}

	FileRep_InsertConfigLogEntry("run resync transition, mark mirror recreated");

	PersistentFileSysObj_MarkMirrorReCreated();

	FileRepSubProcess_ProcessSignals();
	if (!(segmentState == SegmentStateReady &&
		  dataState == DataStateInResync))
	{
		goto exit;
	}

	SIMPLE_FAULT_INJECTOR(FileRepTransitionToInResyncMarkCompleted);

	FileRep_InsertConfigLogEntry("run resync transition, mark transition to resync completed");

	ChangeTracking_MarkTransitionToResyncCompleted();

	FileRepSubProcess_ProcessSignals();
	if (!(segmentState == SegmentStateReady &&
		  dataState == DataStateInResync))
	{
		goto exit;
	}

exit:
	LWLockRelease(MirroredLock);

	/* database is resumed */
	primaryMirrorSetIOSuspended(FALSE);

	return status;
}

/*
 * FileRepPrimary_RunResyncManager()
 */
static int
FileRepPrimary_RunResyncManager(void)
{
	int			status = STATUS_OK;
	bool		retval;
	int			ii = 0;
	bool		mirroredLock = FALSE;
	struct timeval resyncTime;

	FileRepResyncHashEntry_s entry;
	ResynchronizeScanToken token;

	FileRep_InsertConfigLogEntry("run resync manager");

	ResynchronizeScanToken_Init(&token);

	/* set start resync time required for reporting */
	gettimeofday(&resyncTime, NULL);

	fileRepResyncShmem->startResyncTime.tv_sec = resyncTime.tv_sec;
	fileRepResyncShmem->startResyncTime.tv_usec = resyncTime.tv_usec;

	while (1)
	{

		FileRepSubProcess_ProcessSignals();
		if (!(FileRepSubProcess_GetState() == FileRepStateReady &&
			  dataState == DataStateInResync))
		{
			break;
		}

		if (FileRepResync_CheckProgress() == FILEREP_MAX_RESYNC_FILES)
		{
			pg_usleep(50000L);
			//50 ms
				continue;
		}

		retval = PersistentFileSysObj_ResynchronizeScan(
														&token,
														&entry.relFileNode,
														&entry.segmentFileNum,
														&entry.relStorageMgr,
														&entry.mirrorDataSynchronizationState,
														&entry.mirrorBufpoolResyncChangedPageCount,
														&entry.mirrorBufpoolResyncCkptLoc,
														&entry.mirrorBufpoolResyncCkptBlockNum,
														&entry.mirrorAppendOnlyLossEof,
														&entry.mirrorAppendOnlyNewEof,
														&entry.persistentTid,
														&entry.persistentSerialNum);

		if (retval == FALSE)
		{
			/* wait that resync workers are completed */
			while (FileRepResync_CheckProgress() > 0)
			{
				pg_usleep(50000L);
				//50 ms

					FileRepSubProcess_ProcessSignals();
				if (!(FileRepSubProcess_GetState() == FileRepStateReady &&
					  dataState == DataStateInResync))
				{
					break;
				}
			}

			if (mirroredLock == TRUE)
			{
				LWLockRelease(MirroredLock);

				mirroredLock = FALSE;
				break;
			}

			if (!(FileRepSubProcess_GetState() == FileRepStateReady &&
				  dataState == DataStateInResync))
			{
				break;
			}

			LWLockAcquire(MirroredLock, LW_EXCLUSIVE);
			mirroredLock = TRUE;

			if (fileRepResyncShmem->appendOnlyCommitCount == 0)
			{

				FileRep_InsertConfigLogEntry("run resync transition, mark append only incremental");

				PersistentFileSysObj_MarkAppendOnlyCatchup();
				ResynchronizeScanToken_Init(&token);

				/*
				 * MirroredLock is not released. AppendOnly Resync has to
				 * finish under MirrorLock.
				 */
				continue;
			}
			else
			{
				LWLockRelease(MirroredLock);
				mirroredLock = FALSE;

				/*
				 * wait that all in progress AppendOnly transactions are
				 * committed and periodically check if more work to do for
				 * resync
				 */
				pg_usleep(100000L);
				//100 ms

					FileRep_InsertConfigLogEntry("run resync transition, mark append only incremental");

				LWLockAcquire(MirroredLock, LW_EXCLUSIVE);
				PersistentFileSysObj_MarkAppendOnlyCatchup();
				LWLockRelease(MirroredLock);

				ResynchronizeScanToken_Init(&token);
				continue;
			}
		}

		if (entry.mirrorDataSynchronizationState == MirroredRelDataSynchronizationState_DataSynchronized ||
			entry.mirrorDataSynchronizationState == MirroredRelDataSynchronizationState_None)
		{
			if (Debug_filerep_print)
			{
				elog(LOG, "Not adding this entry to hash table %s as DataSynchronized/None %d",
					 entry.fileName, entry.mirrorDataSynchronizationState);
			}
			continue;
		}

		if (isFullResync())
		{
			if (entry.mirrorDataSynchronizationState == MirroredRelDataSynchronizationState_BufferPoolPageIncremental)
			{
				ereport(WARNING,
						(errmsg("resync failure, "
								"unexpected mirror synchronization state during full resynchronization, "
								"failover requested, "
								"relation path to be resynchronized '%s' "
								"relation storage manager '%s(%d)' mirror synchronization state '%s(%d)' "
								"append only loss eof " INT64_FORMAT " "
								"append only new eof " INT64_FORMAT " "
								"mirror buffer pool resync changed page count " INT64_FORMAT " ",
								entry.fileName,
								PersistentFileSysRelStorageMgr_Name(entry.relStorageMgr),
								entry.relStorageMgr,
								MirroredRelDataSynchronizationState_Name(entry.mirrorDataSynchronizationState),
								entry.mirrorDataSynchronizationState,
								entry.mirrorAppendOnlyLossEof,
								entry.mirrorAppendOnlyNewEof,
								entry.mirrorBufpoolResyncChangedPageCount),
						 errhint("run gprecoverseg -F again to re-establish mirror connectivity")));

				FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
				FileRepSubProcess_ProcessSignals();
			}
		}

		/*
		 * Resynchronize Lock is taken for particular relation to protect from
		 * drop and truncate for particular relation.
		 */
		LockRelationForResynchronize(
									 &entry.relFileNode,
									 AccessExclusiveLock);

		if (!PersistentFileSysObj_ResynchronizeRefetch(
													   &entry.relFileNode,
													   &entry.segmentFileNum,
													   &entry.persistentTid,
													   entry.persistentSerialNum,
													   &entry.relStorageMgr,
													   &entry.mirrorDataSynchronizationState,
													   &entry.mirrorBufpoolResyncCkptLoc,
													   &entry.mirrorBufpoolResyncCkptBlockNum,
													   &entry.mirrorAppendOnlyLossEof,
													   &entry.mirrorAppendOnlyNewEof))
		{
			UnlockRelationForResynchronize(
										   &entry.relFileNode,
										   AccessExclusiveLock);

			if (Debug_filerep_print)
				elog(LOG, "Not adding this entry to hash table %s", entry.fileName);
			continue;
		}

		SIMPLE_FAULT_INJECTOR(FileRepResync);

		FileRep_GetRelationPath(
								entry.fileName,
								entry.relFileNode,
								entry.segmentFileNum);

		status = FileRepResync_InsertEntry(&entry);

		if (status != STATUS_OK)
		{
			/*
			 * UnlockRelationForResynchronize() will be issued in
			 * FileRepResync_Cleanup().
			 */
			FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
			if (mirroredLock == TRUE)
			{
				LWLockRelease(MirroredLock);

				mirroredLock = FALSE;
			}
		}

		if (Debug_filerep_config_print)
		{
			elog(LOG,
				 "resync scheduled %s index %d rel storage mgr %s(%d) mirror sync state:%s(%d) "
				 "ao loss eof " INT64_FORMAT " ao new eof " INT64_FORMAT " changed page count " INT64_FORMAT " "
				 "resync ckpt lsn %s resync ckpt blkno %u TID %s serial num " INT64_FORMAT "",
				 entry.fileName,
				 ii++,
				 PersistentFileSysRelStorageMgr_Name(entry.relStorageMgr),
				 entry.relStorageMgr,
				 MirroredRelDataSynchronizationState_Name(entry.mirrorDataSynchronizationState),
				 entry.mirrorDataSynchronizationState,
				 entry.mirrorAppendOnlyLossEof,
				 entry.mirrorAppendOnlyNewEof,
				 entry.mirrorBufpoolResyncChangedPageCount,
				 XLogLocationToString(&entry.mirrorBufpoolResyncCkptLoc),
				 entry.mirrorBufpoolResyncCkptBlockNum,
				 ItemPointerToString(&entry.persistentTid),
				 entry.persistentSerialNum);
		}
	}

	FileRepResync_Cleanup();

	return status;
}

/*
 * FileRepResyncManager_InSyncTransition()
 */
static int
FileRepResyncManager_InSyncTransition(void)
{
	int			status = STATUS_OK;

	FileRep_InsertConfigLogEntry("run resync sync transition");

	SIMPLE_FAULT_INJECTOR(FileRepTransitionToInSyncBegin);

	while (1)
	{
		/*
		 * (MirroredLock, LW_EXCLUSIVE) is acquired and released in
		 * CreateCheckPoint
		 */
		FileRep_InsertConfigLogEntry("run sync transition, request checkpoint");

		MirroredFlatFile_DropFilesFromDir();

		RequestCheckpoint(CHECKPOINT_IMMEDIATE | CHECKPOINT_FORCE | CHECKPOINT_WAIT | CHECKPOINT_RESYNC_TO_INSYNC_TRANSITION);

		/*
		 * The second checkpoint is required in order to mirror pg_control
		 * with last checkpoint position in the xlog file that is mirrored
		 * (XLogSwitch).
		 */
		RequestCheckpoint(CHECKPOINT_IMMEDIATE | CHECKPOINT_FORCE | CHECKPOINT_WAIT);

		FileRepSubProcess_ProcessSignals();
		if (!(FileRepSubProcess_GetState() == FileRepStateReady &&
			  dataState == DataStateInSync))
		{
			break;
		}

		FileRep_InsertConfigLogEntry("run sync transition, mirror to sync transition");

		FileRepPrimary_MirrorInSyncTransition();

		FileRep_InsertConfigLogEntry("run sync transition, mark transition to sync completed");

		ChangeTracking_MarkTransitionToInsyncCompleted();

		FileRep_InsertConfigLogEntry("run sync transition, mark transition to sync completed on primary and mirror");

		/* primary and mirror have now completed re-sync */
		setResyncCompleted();
		primaryMirrorSetInSync();

		SIMPLE_FAULT_INJECTOR(FileRepTransitionToInSyncMarkCompleted);

		break;
	}

	return status;
}

/*
 *
 */
void
FileRepResyncManager_ResyncFlatFiles(void)
{
	int			status = STATUS_OK;

	FileRep_SetSegmentState(SegmentStateInSyncTransition, FaultTypeNotInitialized);

	SIMPLE_FAULT_INJECTOR(FileRepTransitionToInSync);

	while (1)
	{
		FileRep_InsertConfigLogEntry("run sync transition, resync pg_control file");

		status = XLogRecoverMirrorControlFile();

		if (status != STATUS_OK)
		{
			break;
		}

		FileRepSubProcess_ProcessSignals();
		if (segmentState != SegmentStateInSyncTransition)
		{
			break;
		}
		MirroredFlatFile_MirrorDropTemporaryFiles();
		FileRep_InsertConfigLogEntry("run sync transition, resync drop temporary files");

		FileRepSubProcess_ProcessSignals();
		if (segmentState != SegmentStateInSyncTransition)
		{
			break;
		}

		status = FlatFilesTemporaryResynchronizeMirror();
		FileRep_InsertConfigLogEntry("run sync transition, resync temporary files");

		if (status != STATUS_OK)
		{
			break;
		}

		FileRepSubProcess_ProcessSignals();
		if (segmentState != SegmentStateInSyncTransition)
		{
			break;
		}

		FileRep_InsertConfigLogEntry("run sync transition, resync pg_database and pg_auth files");

		status = FlatFilesRecoverMirror();

		if (status != STATUS_OK)
		{
			break;
		}

		FileRepSubProcess_ProcessSignals();
		if (segmentState != SegmentStateInSyncTransition)
		{
			break;
		}

		FileRep_InsertConfigLogEntry("run sync transition, resync pg_twophase files");

		status = TwoPhaseRecoverMirror();

		if (status != STATUS_OK)
		{
			break;
		}

		FileRepSubProcess_ProcessSignals();
		if (segmentState != SegmentStateInSyncTransition)
		{
			break;
		}

		FileRep_InsertConfigLogEntry("run sync transition, resync slru files");

		status = SlruRecoverMirror();

		if (status != STATUS_OK)
		{
			break;
		}

		FileRepSubProcess_ProcessSignals();
		if (segmentState != SegmentStateInSyncTransition)
		{
			break;
		}

		FileRep_InsertConfigLogEntry("run sync transition, resync pgversion files");

		status = PgVersionRecoverMirror();

		if (status != STATUS_OK)
		{
			break;
		}

		FileRepSubProcess_ProcessSignals();

		FileRep_InsertConfigLogEntry("run sync transition, resync pgxlog files");

		status = XLogRecoverMirror();

		if (status != STATUS_OK)
		{
			break;
		}

		FileRepSubProcess_ProcessSignals();

		break;
	}

	if (status != STATUS_OK)
	{
		FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
		FileRepSubProcess_ProcessSignals();
	}

	if (segmentState == SegmentStateInSyncTransition &&
		status == STATUS_OK)
	{
		FileRep_SetDataState(DataStateInSync, FALSE);

		FileRep_SetSegmentState(SegmentStateReady, FaultTypeNotInitialized);
	}

	return;
}

static int
FileRepResync_InsertEntry(
						  FileRepResyncHashEntry_s *entry)
{
	int			status = STATUS_OK;
	bool		foundPtr;
	FileRepResyncHashEntry_s *entryLocal;
	char		key[MAXPGPATH + 1];

	snprintf(key, sizeof(key), "%s", entry->fileName);

	if (Debug_filerep_print)
		elog(LOG, "FileRepResync_InsertEntry() identifier:'%s'  ", key);

	FileRepResync_LockAcquire();

	Assert(fileRepResyncShmem->fileRepResyncHash != NULL);

	entryLocal = (FileRepResyncHashEntry_s *) hash_search(
														  fileRepResyncShmem->fileRepResyncHash,
														  (void *) &key,
														  HASH_ENTER_NULL,
														  &foundPtr);

	if (entryLocal == NULL)
	{

		status = STATUS_ERROR;
		ereport(WARNING,
				(errmsg("resync failure, "
						"could not insert resync information into hash table identifier '%s', no memory "
						"failover requested",
						key),
				 errhint("run gprecoverseg to re-establish mirror connectivity"),
				 FileRep_errcontext()));
		goto exit;
	}

	if (foundPtr)
	{

		status = STATUS_ERROR;
		ereport(WARNING,
				(errmsg("resync failure, "
						"could not insert resync information into hash table identifier '%s', entry exists "
						"failover requested",
						key),
				 errhint("run gprecoverseg to re-establish mirror connectivity"),
				 FileRep_errcontext()));


	}
	else
	{

		entryLocal->relFileNode.relNode = entry->relFileNode.relNode;
		entryLocal->relFileNode.spcNode = entry->relFileNode.spcNode;
		entryLocal->relFileNode.dbNode = entry->relFileNode.dbNode;
		entryLocal->segmentFileNum = entry->segmentFileNum;

		entryLocal->relStorageMgr = entry->relStorageMgr;
		entryLocal->mirrorDataSynchronizationState = entry->mirrorDataSynchronizationState;
		entryLocal->mirrorBufpoolResyncCkptLoc = entry->mirrorBufpoolResyncCkptLoc;
		entryLocal->mirrorBufpoolResyncCkptBlockNum = entry->mirrorBufpoolResyncCkptBlockNum;

		entryLocal->mirrorAppendOnlyLossEof = entry->mirrorAppendOnlyLossEof;
		entryLocal->mirrorAppendOnlyNewEof = entry->mirrorAppendOnlyNewEof;

		entryLocal->mirrorBufpoolResyncChangedPageCount = entry->mirrorBufpoolResyncChangedPageCount;

		entryLocal->persistentTid = entry->persistentTid;
		entryLocal->persistentSerialNum = entry->persistentSerialNum;

		entryLocal->fileRepResyncState = FileRepResyncStateInitialized;

		fileRepResyncShmem->writeCount++;

	}

exit:
	FileRepResync_LockRelease();

	return status;
}


static FileRepResyncHashEntry_s *
FileRepResync_LookupEntry(FileName fileName)
{
	FileRepResyncHashEntry_s *entry;
	char		key[MAXPGPATH + 1];
	bool		foundPtr;

	snprintf(key, sizeof(key), "%s", fileName);

	Assert(fileRepResyncShmem->fileRepResyncHash != NULL);

	entry = (FileRepResyncHashEntry_s *) hash_search(
													 fileRepResyncShmem->fileRepResyncHash,
													 (void *) &key,
													 HASH_ENTER_NULL,
													 &foundPtr);

	if (Debug_filerep_print)
	{
		if (entry == NULL)
			elog(LOG, "FileRepResync_LookupEntry() could not find resync entry identifier:'%s' ", fileName);
	}

	return entry;
}

static bool
FileRepResync_RemoveEntry(FileName fileName)
{

	FileRepResyncHashEntry_s *entry;
	char		key[MAXPGPATH + 1];
	bool		isRemoved = FALSE;

	snprintf(key, sizeof(key), "%s", fileName);

	Assert(fileRepResyncShmem->fileRepResyncHash != NULL);

	entry = hash_search(fileRepResyncShmem->fileRepResyncHash,
						(void *) &key,
						HASH_REMOVE,
						NULL);

	if (entry)
	{
		if (Debug_filerep_print)
			elog(LOG, "FileRepResync_RemoveEntry() removed resync entry identifier:'%s' ", fileName);
		isRemoved = TRUE;
	}

	return isRemoved;
}

static int
FileRepResync_CheckProgress(void)
{
	FileRepResyncHashEntry_s *entry = NULL;
	HASH_SEQ_STATUS hash_status;
	int			countProgress = 0;

	FileRepResync_LockAcquire();

	Assert(fileRepResyncShmem->fileRepResyncHash != NULL);

	if (fileRepResyncShmem->resyncCompletedCount > 0)
	{

		hash_seq_init(&hash_status, fileRepResyncShmem->fileRepResyncHash);

		while ((entry = (FileRepResyncHashEntry_s *) hash_seq_search(&hash_status)) != NULL)
		{
			switch (entry->fileRepResyncState)
			{
				case FileRepResyncStateInitialized:
				case FileRepResyncStateInProgress:

					break;

				case FileRepResyncStateCompleted:

					/*
					 * Release Resynchronize Lock on relation that was taken
					 * before fetching the relation
					 */
					UnlockRelationForResynchronize(
												   &entry->relFileNode,
												   AccessExclusiveLock);

					if (FileRepResync_RemoveEntry(entry->fileName) == TRUE)
					{
						fileRepResyncShmem->resyncCompletedCount--;
						Assert(fileRepResyncShmem->resyncCompletedCount >= 0);
					}
					else
					{
						Assert(0);
					}

					fileRepResyncShmem->blocksSynchronized += entry->mirrorBufpoolResyncChangedPageCount;

					/*
					 * PersistentFileSysObj_ResynchronizeRelationComplete()
					 * issues IOs. That may be long operation especially if IO
					 * is issued while segment is transitioning to change
					 * tracking.
					 */
					FileRepResync_LockRelease();

					if (Debug_filerep_print)
					{
						elog(LOG, "FileRepResync_CheckProgress() identifier:'%s' state:'%d' resyncCompletedCount:'%d' "
							 "blocks synchronized: " INT64_FORMAT " ",
							 entry->fileName,
							 entry->fileRepResyncState,
							 fileRepResyncShmem->resyncCompletedCount,
							 fileRepResyncShmem->blocksSynchronized);
					}

					PersistentFileSysObj_ResynchronizeRelationComplete(
																	   &entry->persistentTid,
																	   entry->persistentSerialNum,
																	   entry->mirrorAppendOnlyNewEof,
																	   TRUE);

					FileRepResync_LockAcquire();

					break;

					/* FileRepResyncStateFault is not in use */
				case FileRepResyncStateFault:
				case FileRepResyncStateNotInitialized:
					Assert(0);
					break;
			}
		}
	}

#ifdef FAULT_INJECTOR
	if (fileRepResyncShmem->resyncInProgressCount > 10)
		FaultInjector_InjectFaultIfSet(
									   FileRepResyncInProgress,
									   DDLNotSpecified,
									   "", //databaseName
									   "");
	//tableName
#endif

		countProgress = fileRepResyncShmem->resyncCompletedCount +
		fileRepResyncShmem->writeCount +
		fileRepResyncShmem->resyncInProgressCount;

	FileRepResync_LockRelease();

	return countProgress;
}

/*
 * This function returns either of two output parameters.  For relations that
 * can be resynchronized using changed blocks recorded in CT log, "request"
 * object is created and returned as output parameter.  These are the relations
 * whose mirrorDataSynchronizationState in persistent table is PageIncremental.
 * E.g. regular heap tables on which insert/update/delete was run while primary
 * was in changetracking.  Second category of relations are appendonly tables
 * or heap tables whose mirrorDataSynchronizationState is ScanIncremental.
 * These are persistent tables and heap tables that were truncated when primary
 * was in changetracking.  Changed blocks recorded in CT log are NOT used for
 * resynchronizing these tables.  The CT hash entry is directly returned in
 * this case.
 */
FileRepResyncHashEntry_s *
FileRepPrimary_GetResyncEntry(ChangeTrackingRequest **request)
{
	bool		found = FALSE;
	FileRepResyncHashEntry_s *entry = NULL;
	HASH_SEQ_STATUS hash_status;
	int			NumberOfRelations = 0;
	ChangeTrackingRequest *requestLocal = NULL;
	int64		changedPageCount = 0;

	FileRepResync_LockAcquire();

	Assert(fileRepResyncShmem->fileRepResyncHash != NULL);

	if (fileRepResyncShmem->writeCount > 0)
	{

		hash_seq_init(&hash_status, fileRepResyncShmem->fileRepResyncHash);

		while ((entry = (FileRepResyncHashEntry_s *) hash_seq_search(&hash_status)) != NULL)
		{
			if (entry->fileRepResyncState != FileRepResyncStateInitialized)
			{
				continue;
			}

			if (entry->mirrorDataSynchronizationState == MirroredRelDataSynchronizationState_BufferPoolPageIncremental)
			{
				changedPageCount += entry->mirrorBufpoolResyncChangedPageCount;

				if (changedPageCount > gp_filerep_ct_batch_size)
				{
					if (NumberOfRelations == 0)
					{
						NumberOfRelations++;
					}
					hash_seq_term(&hash_status);
					break;
				}

				NumberOfRelations++;
			}
			else
			{
				if (NumberOfRelations > 0)
				{
					/*
					 * if first entry has BufferPoolPageIncremental state then
					 * group only relations with BufferPoolPageIncremental for
					 * next resync.
					 */
					continue;
				}
				hash_seq_term(&hash_status);
				found = TRUE;
				entry->fileRepResyncState = FileRepResyncStateInProgress;
				fileRepResyncShmem->writeCount--;
				fileRepResyncShmem->resyncInProgressCount++;

				Assert(fileRepResyncShmem->writeCount >= 0);

				if (Debug_filerep_print)
					elog(LOG,
						 "FileRepPrimary_GetResyncEntry() identifier:'%s' "
						 "mirrorDataSynchronizationState:'%s(%d)' ",
						 entry->fileName,
						 MirroredRelDataSynchronizationState_Name(entry->mirrorDataSynchronizationState),
						 entry->mirrorDataSynchronizationState);

				break;
			}
		}

		if (NumberOfRelations > 0)
		{
			int			count = 0;

			requestLocal = ChangeTracking_FormRequest(NumberOfRelations);

			hash_seq_init(&hash_status, fileRepResyncShmem->fileRepResyncHash);

			while (count < NumberOfRelations &&
				   (entry = (FileRepResyncHashEntry_s *) hash_seq_search(&hash_status)) != NULL)
			{
				if (entry->fileRepResyncState != FileRepResyncStateInitialized)
				{
					continue;
				}

				if (entry->mirrorDataSynchronizationState == MirroredRelDataSynchronizationState_BufferPoolPageIncremental)
				{

					/*
					 * When a single relation has more changed blocks than
					 * gp_filerep_ct_batch_size, it must be called in
					 * ChangeTracking_GetChanges() by itself (with no other
					 * relation).  The GetChanges() routine will return the
					 * first batch of changes, plus an indication that
					 * GetChanges() did not finish with this relation
					 * (ask_for_more flag).  The caller must invoke
					 * GetChanges() for this relation again (when ready) and
					 * pass the last block number from the previous call as
					 * the beginning block number for this call.  GetChanges()
					 * will then return the next batch of changes and this
					 * will continue until ask_for_more flag is returned as
					 * false.
					 *
					 * If GetChanges() was called with more than 1 relation
					 * (persistent serial number) at a time AND it sees more
					 * than gp_filerep_ct_batch_size changes it will be an
					 * internal error.
					 */
					if (entry->mirrorBufpoolResyncChangedPageCount > gp_filerep_ct_batch_size)
					{
						Assert(NumberOfRelations == 1);
					}

					ChangeTracking_AddRequestEntry(requestLocal, entry->relFileNode);
					found = TRUE;
					fileRepResyncShmem->writeCount--;
					fileRepResyncShmem->resyncInProgressCount++;
					entry->fileRepResyncState = FileRepResyncStateInProgress;
					count++;

					Assert(fileRepResyncShmem->writeCount >= 0);

					if (Debug_filerep_print)
						elog(LOG,
							 "FileRepPrimary_GetResyncEntry() identifier:'%s' NumberOfRelations:'%d' "
							 "mirrorDataSynchronizationState:'%s(%d)' count:'%d' ",
							 entry->fileName,
							 NumberOfRelations,
							 MirroredRelDataSynchronizationState_Name(entry->mirrorDataSynchronizationState),
							 entry->mirrorDataSynchronizationState,
							 count);
				}
			}
			if (entry != NULL)
			{
				hash_seq_term(&hash_status);
				entry = NULL;
			}
			Insist(requestLocal->count == NumberOfRelations);
			*request = requestLocal;

		}
	}

	FileRepResync_LockRelease();

	if (found == FALSE)
	{
		entry = NULL;
	}

	return entry;
}

int
FileRepResync_UpdateEntry(
						  FileRepResyncHashEntry_s *entry)
{
	FileRepResyncHashEntry_s *entryLocal;
	int			status = STATUS_OK;

	FileRepResync_LockAcquire();

	entryLocal = FileRepResync_LookupEntry(entry->fileName);

	if (entryLocal != NULL)
	{
		fileRepResyncShmem->resyncCompletedCount++;
		fileRepResyncShmem->resyncInProgressCount--;

		entryLocal->fileRepResyncState = FileRepResyncStateCompleted;

		if (entryLocal->mirrorBufpoolResyncChangedPageCount == 0)
		{
			entryLocal->mirrorBufpoolResyncChangedPageCount = entry->mirrorBufpoolResyncChangedPageCount;
		}

		Assert(fileRepResyncShmem->resyncInProgressCount >= 0);

	}
	else
	{
		Assert(0);
		status = STATUS_ERROR;
	}

	if (Debug_filerep_print)
	{
		elog(LOG, "FileRepResync_UpdateEntry() identifier:'%s' state:'%d' resyncCompletedCount:'%d' ",
			 entry->fileName,
			 entry->fileRepResyncState,
			 fileRepResyncShmem->resyncCompletedCount);
	}

	FileRepResync_LockRelease();

	return status;
}
