/*-------------------------------------------------------------------------
 *
 * cdbfilerepprimary.c
 *
 * Portions Copyright (c) 2009-2010 Greenplum Inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbfilerepprimary.c
 *
 *-------------------------------------------------------------------------
 */

/*
 * FileRep process (sender thread) launched by main thread.
 * Responsibilities of this module.
 *		*)
 *
 */
#include "postgres.h"

#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

#include "miscadmin.h"

#include "access/xlog_internal.h"
#include "cdb/cdbfilerepservice.h"
#include "cdb/cdbfilerepprimary.h"
#include "cdb/cdbfilerepprimaryack.h"
#include "cdb/cdbfilerepconnclient.h"
#include "cdb/cdbfilerepresyncmanager.h"
#include "cdb/cdbvars.h"
#include "storage/lwlock.h"
#include "storage/pmsignal.h"
#include "storage/ipc.h"
#include "postmaster/primary_mirror_mode.h"
#include "utils/faultinjector.h"
#include "utils/memutils.h"

static bool FileRepPrimary_IsMirroringRequired(
								   FileRepRelationType_e fileRepRelationType,
								   FileRepOperation_e fileRepOperation);

/****************************************************************
 * SENDER THREAD definitions
 ****************************************************************/

static int	FileRepPrimary_RunSender(void);
static int FileRepPrimary_ConstructAndInsertMessage(
										 FileRepIdentifier_u fileRepIdentifier,
										 FileRepRelationType_e fileRepRelationType,
										 FileRepOperation_e fileRepOperation,
										 FileRepOperationDescription_u fileRepOperationDescription,
										 char *data,
										 uint32 dataLength);

/****************************************************************
 * Routines called by BACKEND PROCESSES
 ****************************************************************/

bool
FileRepPrimary_IsResyncWorker(void)
{
	return (
			fileRepProcessType == FileRepProcessTypeResyncWorker1 ||
			fileRepProcessType == FileRepProcessTypeResyncWorker2 ||
			fileRepProcessType == FileRepProcessTypeResyncWorker3 ||
			fileRepProcessType == FileRepProcessTypeResyncWorker4);
}

bool
FileRepPrimary_IsResyncManagerOrWorker(void)
{
	return (
			fileRepProcessType == FileRepProcessTypeResyncManager ||
			fileRepProcessType == FileRepProcessTypeResyncWorker1 ||
			fileRepProcessType == FileRepProcessTypeResyncWorker2 ||
			fileRepProcessType == FileRepProcessTypeResyncWorker3 ||
			fileRepProcessType == FileRepProcessTypeResyncWorker4);
}

MirrorDataLossTrackingState
FileRepPrimary_GetMirrorDataLossTrackingSessionNum(int64 *sessionNum)
{

	*sessionNum = getChangeTrackingSessionId();

	getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);

	switch (dataState)
	{
		case DataStateNotInitialized:
			return MirrorDataLossTrackingState_MirrorNotConfigured;

		case DataStateInResync:
			if (!FileRepResync_IsReMirrorAllowed() &&
				(segmentState == SegmentStateInitialization ||
				 segmentState == SegmentStateInResyncTransition))
				return MirrorDataLossTrackingState_MirrorDown;
			else
				return MirrorDataLossTrackingState_MirrorCurrentlyUpInResync;

		case DataStateInSync:
			return MirrorDataLossTrackingState_MirrorCurrentlyUpInSync;

		case DataStateInChangeTracking:

			/*
			 * If segment is in transition to Change Tracking then IO activity
			 * must be suspended.
			 */
			if (fileRepProcessType != FileRepProcessTypePrimaryRecovery)
			{
				FileRepPrimary_IsMirroringRequired(
												   FileRepRelationTypeNotSpecified,
												   FileRepOperationNotSpecified);
			}

			return MirrorDataLossTrackingState_MirrorDown;

		default:
			Assert(0);
			return MirrorDataLossTrackingState_MirrorNotConfigured;
	}
	return MirrorDataLossTrackingState_MirrorNotConfigured;
}

bool
FileRepPrimary_IsMirrorDataLossOccurred(void)
{
	getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);

	return ((dataState == DataStateInChangeTracking) ? TRUE : FALSE);
}

int
FileRepPrimary_IntentAppendOnlyCommitWork(void)
{
	return FileRepResync_IncAppendOnlyCommitCount();
}

int
FileRepPrimary_FinishedAppendOnlyCommitWork(int count)
{
	return FileRepResync_DecAppendOnlyCommitCount(count);
}

int
FileRepPrimary_GetAppendOnlyCommitWorkCount(void)
{
	return FileRepResync_GetAppendOnlyCommitCount();
}

/*
 *
 * If (dataState == Change Logging) then mirroring is not required.
 * If (dataState == InSync or InResync) then mirroring is required
 * except when segmentState is in Fault.
 * In that case the request is on hold. It is waiting on third coordinator
 * to decide about the next action (failover, stop, ...).
 */
static bool
FileRepPrimary_IsMirroringRequired(
								   FileRepRelationType_e fileRepRelationType,
								   FileRepOperation_e fileRepOperation)
{
	bool		isMirroringRequired = FALSE;
	int			report = FALSE;
	bool		isInTransition = FALSE;
	DataState_e dataStateTransition;

	while (1)
	{
		getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, &isInTransition, &dataStateTransition);

		switch (fileRepRole)
		{
			case FileRepRoleNotInitialized:
				Insist(0);
				break;
			case FileRepMirrorRole:
				Insist(0);
				break;
			case FileRepPrimaryRole:
				Insist(dataState != DataStateNotInitialized);
				break;
			case FileRepNoRoleConfigured:
				Insist(dataState == DataStateNotInitialized);
				break;
			default:
				Insist(0);
				break;
		}

		switch (dataState)
		{
			case DataStateNotInitialized:
				/* Insist(0); */
				break;

			case DataStateInResync:
				if (!FileRepResync_IsReMirrorAllowed())
				{
					if (segmentState == SegmentStateInitialization ||
						segmentState == SegmentStateInResyncTransition)
					{
						break;
					}
				}
				/* no break */
			case DataStateInSync:

				/*
				 * FileRep backend processes have to exit in order to be able
				 * to transition to Change Tracking.
				 */
				if (segmentState == SegmentStateShutdownFilerepBackends &&
					FileRepIsBackendSubProcess(fileRepProcessType))
				{
					break;
				}

				/*
				 * If mirror is loss then backends should abort during
				 * shutdown in order to avoid writing to local side (i.e.
				 * checkpoint occur on primary and not on mirror)
				 */
				if (segmentState == SegmentStateShutdownFilerepBackends &&
					fileRepShmemArray[0]->state == FileRepStateFault &&
					fileRepProcessType == FileRepProcessTypeNotInitialized &&
					!(isInTransition == TRUE &&
					  dataStateTransition == DataStateInChangeTracking))
				{
					ereport(LOG,
							(errmsg("failure is detected in segment mirroring during backend shutdown, abort requested, "
									"mirroring role '%s' "
									"mirroring state '%s' "
									"segment state '%s' "
									"filerep state '%s' "
									"process name(pid) '%s(%d)' ",
									FileRepRoleToString[fileRepRole],
									DataStateToString[dataState],
									SegmentStateToString[segmentState],
									FileRepStateToString[FileRepSubProcess_GetState()],
									FileRepProcessTypeToString[fileRepProcessType],
									getpid())));

					FileRep_InsertConfigLogEntry("failure is detected in segment mirroring during backend shutdown, abort requested");
					LWLockReleaseAll();
					proc_exit(0);
				}

				if (segmentState != SegmentStateFault &&
					!(isInTransition == TRUE &&
					  dataStateTransition == DataStateInChangeTracking))
				{
					isMirroringRequired = TRUE;
				}
				else
				{
					/*
					 * shutdown request has to be let through in order to
					 * transition to change tracking
					 */
					if (FileRep_IsIpcSleep(fileRepOperation))
					{
						break;
					}

					if (!report)
					{
						ereport(LOG,
								(errmsg("failure is detected in segment mirroring, "
										"failover requested "),
								 FileRep_errcontext()));
						report = TRUE;

						/*
						 * database transitions to suspended state, IO
						 * activity on the segment is suspended
						 */
						primaryMirrorSetIOSuspended(TRUE);
					}

					/*
					 * This code path can be reached from backends which are
					 * not directly spawned from PostMaster.  For e.g the
					 * FileRepResyncWorker process is spawned by FileRepMain.
					 * Hence PostmasterIsAlive() needs to be called with
					 * false.
					 */
					if (!PostmasterIsAlive(false))
					{
						LockReleaseAll(DEFAULT_LOCKMETHOD, false);
						LWLockReleaseAll();
						proc_exit(0);
					}
					pg_usleep(500000L); /* 500 ms */
					continue;
				}
				break;

			case DataStateInChangeTracking:
				if ((segmentState == SegmentStateInitialization ||
					 segmentState == SegmentStateInChangeTrackingTransition) &&
					fileRepRelationType != FileRepRelationTypeFlatFile)
				{
					/*
					 * shutdown request has to be let through in order to
					 * recover
					 */
					if (FileRep_IsIpcSleep(fileRepOperation))
					{
						break;
					}

					pg_usleep(500000L); /* 500 ms */
					continue;
				}

				if (report)
				{
					ereport(LOG,
							(errmsg("failover was performed to the primary segment, "
									"segment mirroring is suspended "),
							 errSendAlert(true),
							 FileRep_errcontext()));

					report = FALSE;

					/* database is resumed */
					primaryMirrorSetIOSuspended(FALSE);
				}
				/* No Operation */
				break;
			default:
				Insist(0);
				break;
		}
		break;
	} //while (1)

		return isMirroringRequired;
}

/*
 * FileRepPrimary_ConstructAndInsertMessage
 *		Reserve shared memory
 *		Construct Message Header for open operation
 *		Construct Message Header Crc
 *		Insert Message into Mirror Messages Shared Memory
 *		Set FileRep Message state to Ready, so message can be consumed.
 */
static int
FileRepPrimary_ConstructAndInsertMessage(
										 FileRepIdentifier_u fileRepIdentifier,
										 FileRepRelationType_e fileRepRelationType,
										 FileRepOperation_e fileRepOperation,
										 FileRepOperationDescription_u fileRepOperationDescription,
										 char *data,
										 uint32 dataLength)
{

	FileRepShmemMessageDescr_s *fileRepShmemMessageDescr;
	FileRepMessageHeader_s *fileRepMessageHeader;
	pg_crc32   *fileRepMessageHeaderCrc;
	pg_crc32	fileRepMessageHeaderCrcLocal;
	char	   *msgPositionInsert = NULL;
	char	   *fileRepMessageBody = NULL;
	uint32		msgLength = 0;
	int			status = STATUS_OK;
	uint32		spareField = 0;

	/*
	 * Prevent cancel/die interrupt while doing this multi-step in order to
	 * insert message and update message state atomically with respect to
	 * cancel/die interrupts. See MPP-10040.
	 */
	HOLD_INTERRUPTS();

	if (FileRep_IsOperationSynchronous(fileRepOperation) == TRUE)
	{

		status = FileRepAckPrimary_NewHashEntry(
												fileRepIdentifier,
												fileRepOperation,
												fileRepRelationType);
	}

	if (status != STATUS_OK)
	{

		RESUME_INTERRUPTS();

		if ((!primaryMirrorIsIOSuspended()) && dataState != DataStateInChangeTracking)
		{
			ereport(WARNING,
					(errmsg("mirror failure, "
							"could not insert message in ACK table"
							"failover requested"),
					 errhint("run gprecoverseg to re-establish mirror connectivity"),
					 FileRep_errdetail(fileRepIdentifier,
									   fileRepRelationType,
									   fileRepOperation,
									   FILEREP_UNDEFINED),
					 FileRep_errdetail_Shmem(),
					 FileRep_errcontext()));

			FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
			FileRepSubProcess_ProcessSignals();
		}

		FileRepPrimary_IsMirroringRequired(
										   fileRepRelationType,
										   fileRepOperation);
		return status;
	}

	msgLength = sizeof(FileRepMessageHeader_s) + sizeof(pg_crc32) + dataLength;
/*
 * Shared memory is reserved in advance in order to avoid
 * holding lock during memcpy() and Crc calculation...
 *
 * Construct FileRepShmemMessageDescr_s
 */
	msgPositionInsert = FileRep_ReserveShmem(
											 fileRepShmemArray[fileRepProcIndex],
											 msgLength,
											 &spareField,
											 fileRepOperation,
											 FileRepShmemLock);

	if (msgPositionInsert == NULL)
	{
		RESUME_INTERRUPTS();

		status = STATUS_ERROR;

		if ((!primaryMirrorIsIOSuspended()) && dataState != DataStateInChangeTracking)
		{
			ereport(WARNING,
					(errmsg("mirror failure, "
							"could not queue message to be mirrored, "
							"failover requested"),
					 errhint("run gprecoverseg to re-establish mirror connectivity"),
					 FileRep_errdetail(fileRepIdentifier,
									   fileRepRelationType,
									   fileRepOperation,
									   FILEREP_UNDEFINED),
					 FileRep_errdetail_Shmem(),
					 FileRep_errcontext()));

			FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
			FileRepSubProcess_ProcessSignals();
		}

		FileRepPrimary_IsMirroringRequired(
										   fileRepRelationType,
										   fileRepOperation);
		return status;
	}

/* Construct FileRepMessageHeader_s */

	fileRepMessageHeader =
		(FileRepMessageHeader_s *) (msgPositionInsert +
									sizeof(FileRepShmemMessageDescr_s));

	fileRepMessageHeader->fileRepMessageHeaderVersion = FileRepMessageHeaderVersionOne;

	fileRepMessageHeader->fileRepRelationType = fileRepRelationType;

	fileRepMessageHeader->fileRepOperation = fileRepOperation;

	fileRepMessageHeader->fileRepIdentifier = fileRepIdentifier;

	fileRepMessageHeader->messageCount = spareField;

	fileRepMessageHeader->messageBodyLength = dataLength;

	switch (fileRepOperation)
	{
		case FileRepOperationReconcileXLogEof:
		case FileRepOperationOpen:
		case FileRepOperationTruncate:
		case FileRepOperationRename:
		case FileRepOperationHeartBeat:
		case FileRepOperationCreateAndOpen:
		case FileRepOperationCreate:
		case FileRepOperationValidation:
		case FileRepOperationStartSlruChecksum:
		case FileRepOperationVerifySlruDirectoryChecksum:
			fileRepMessageHeader->fileRepOperationDescription = fileRepOperationDescription;
			break;

		default:
			break;
	}

	if (dataLength > 0)
	{
		FileRep_CalculateCrc((char *) data,
							 dataLength,
							 &fileRepMessageHeader->fileRepMessageBodyCrc);
	}

	/*
	 * The following fields are not in use during open(). They were zeroed
	 * during reserving message buffer. a) fileRepOperationDescription b)
	 * fileRepMessageBodyCrc c) messageCount
	 */

/* Construct FileRepShmemMessageHeaderCrc */

	fileRepMessageHeaderCrc =
		(pg_crc32 *) (msgPositionInsert +
					  sizeof(FileRepMessageHeader_s) +
					  sizeof(FileRepShmemMessageDescr_s));

	FileRep_CalculateCrc((char *) fileRepMessageHeader,
						 sizeof(FileRepMessageHeader_s),
						 &fileRepMessageHeaderCrcLocal);

	*fileRepMessageHeaderCrc = fileRepMessageHeaderCrcLocal;

	FileRep_InsertLogEntry(
						   "P_ConstructAndInsertMessage",
						   fileRepIdentifier,
						   fileRepRelationType,
						   fileRepOperation,
						   fileRepMessageHeaderCrcLocal,
						   FILEREP_UNDEFINED,
						   FileRepAckStateNotInitialized,
						   FILEREP_UNDEFINED,
						   fileRepMessageHeader->messageCount);

	if (dataLength > 0)
	{
		fileRepMessageBody = (char *) (msgPositionInsert +
									   sizeof(FileRepMessageHeader_s) +
									   sizeof(FileRepShmemMessageDescr_s) +
									   sizeof(pg_crc32));

		memcpy(fileRepMessageBody, data, dataLength);
	}

	fileRepShmemMessageDescr =
		(FileRepShmemMessageDescr_s *) msgPositionInsert;

	fileRepShmemMessageDescr->messageSync =
		FileRep_IsOperationSynchronous(fileRepOperation);

	Assert(status == STATUS_OK);

	fileRepShmemMessageDescr->messageState = FileRepShmemMessageStateReady;

	LWLockAcquire(FileRepShmemLock, LW_EXCLUSIVE);
	FileRep_IpcSignal(fileRepIpcArray[fileRepShmemArray[fileRepProcIndex]->ipcArrayIndex]->semC,
					  &fileRepIpcArray[fileRepShmemArray[fileRepProcIndex]->ipcArrayIndex]->refCountSemC);
	LWLockRelease(FileRepShmemLock);

	RESUME_INTERRUPTS();

	return status;
}

/*
 * FileRepPrimary_MirrorOpen
 */
int
FileRepPrimary_MirrorOpen(FileRepIdentifier_u fileRepIdentifier,
						  FileRepRelationType_e fileRepRelationType,
						  int64 logicalEof,
						  int fileFlags,
						  int fileMode,
						  bool suppressError)
{
	int			status = STATUS_OK;
	FileRepOperation_e fileRepOperation = FileRepOperationOpen;
	bool		mirrorCreateAndOpen = FALSE;
	FileRepOperationDescription_u descr;

	descr.open.fileFlags = fileFlags;

	descr.open.fileMode = fileMode;
	descr.open.logicalEof = logicalEof;
	descr.open.suppressError = suppressError;

	if ((fileFlags & O_CREAT) == O_CREAT)
	{
		fileRepOperation = FileRepOperationCreateAndOpen;
	}

	if (FileRepPrimary_IsMirroringRequired(fileRepRelationType, fileRepOperation))
	{
		if (!(dataState == DataStateInResync &&
			  fileRepRelationType == FileRepRelationTypeFlatFile &&
			  segmentState != SegmentStateInSyncTransition))
		{
			status = FileRepPrimary_ConstructAndInsertMessage(
															  fileRepIdentifier,
															  fileRepRelationType,
															  fileRepOperation,
															  descr,
															  NULL, /* data */
															  0);	/* data length */
			if (fileRepOperation == FileRepOperationCreateAndOpen &&
				status == STATUS_OK)
			{
				mirrorCreateAndOpen = TRUE;
			}
		}
	}

	if (mirrorCreateAndOpen)
	{
		if (FileRepPrimary_IsOperationCompleted(
												fileRepIdentifier,
												fileRepRelationType) == FALSE)
		{
			status = STATUS_ERROR;
		}
	}

	return status;
}

/*
 * FileRepPrimary_MirrorFlush
 */
int
FileRepPrimary_MirrorFlush(FileRepIdentifier_u fileRepIdentifier,
						   FileRepRelationType_e fileRepRelationType)
{
	int			status = STATUS_OK;

	FileRepOperationDescription_u descr;

	if (FileRepPrimary_IsMirroringRequired(fileRepRelationType, FileRepOperationFlush))
	{
		if (!(dataState == DataStateInResync &&
			  fileRepRelationType == FileRepRelationTypeFlatFile &&
			  segmentState != SegmentStateInSyncTransition))
		{
#ifdef FAULT_INJECTOR
			if (FileRepPrimary_IsResyncWorker())
			{
				FaultInjector_InjectFaultIfSet(
											   FileRepIsOperationCompleted,
											   DDLNotSpecified,
											   "", //databaseName
											   "");
				/* tableName */
			}
#endif
			/* Just to make compiler happy */
			descr.defaultDesc.placeholder = false;

			status = FileRepPrimary_ConstructAndInsertMessage(
															  fileRepIdentifier,
															  fileRepRelationType,
															  FileRepOperationFlush,
															  descr,
															  NULL, /* data */
															  0);	/* data length */
		}
	}

	return status;
}

/*
 * FileRepPrimary_MirrorClose
 */
int
FileRepPrimary_MirrorClose(FileRepIdentifier_u fileRepIdentifier,
						   FileRepRelationType_e fileRepRelationType)
{
	int			status = STATUS_OK;

	FileRepOperationDescription_u descr;

	if (FileRepPrimary_IsMirroringRequired(fileRepRelationType, FileRepOperationClose))
	{
		if (!(dataState == DataStateInResync &&
			  fileRepRelationType == FileRepRelationTypeFlatFile &&
			  segmentState != SegmentStateInSyncTransition))
		{
			/* Just to make compiler happy */
			descr.defaultDesc.placeholder = false;

			status = FileRepPrimary_ConstructAndInsertMessage(
															  fileRepIdentifier,
															  fileRepRelationType,
															  FileRepOperationClose,
															  descr,
															  NULL, /* data */
															  0);	/* data length */
		}
	}

	return status;
}

/*
 * FileRepPrimary_MirrorFlushAndClose
 */
int
FileRepPrimary_MirrorFlushAndClose(FileRepIdentifier_u fileRepIdentifier,
								   FileRepRelationType_e fileRepRelationType)
{
	int			status = STATUS_OK;

	FileRepOperationDescription_u descr;

	if (FileRepPrimary_IsMirroringRequired(fileRepRelationType, FileRepOperationFlushAndClose))
	{
		if (!(dataState == DataStateInResync &&
			  fileRepRelationType == FileRepRelationTypeFlatFile &&
			  segmentState != SegmentStateInSyncTransition))
		{
			/* Just to make compiler happy */
			descr.defaultDesc.placeholder = false;

			status = FileRepPrimary_ConstructAndInsertMessage(
															  fileRepIdentifier,
															  fileRepRelationType,
															  FileRepOperationFlushAndClose,
															  descr,
															  NULL, /* data */
															  0);	/* length */
		}
	}

	return status;
}

/*
 * FileRepPrimary_MirrorTruncate
 */
int
FileRepPrimary_MirrorTruncate(
							  FileRepIdentifier_u fileRepIdentifier,
 /* */

							  FileRepRelationType_e fileRepRelationType,
 /* */

							  int64 position)
{
	int			status = STATUS_OK;

	FileRepOperationDescription_u descr;

	descr.truncate.position = position;

	if (FileRepPrimary_IsMirroringRequired(fileRepRelationType, FileRepOperationTruncate))
	{
		if (!(dataState == DataStateInResync &&
			  fileRepRelationType == FileRepRelationTypeFlatFile &&
			  segmentState != SegmentStateInSyncTransition))
		{
			status = FileRepPrimary_ConstructAndInsertMessage(
															  fileRepIdentifier,
															  fileRepRelationType,
															  FileRepOperationTruncate,
															  descr,
															  NULL, /* data */
															  0);	/* length */
		}
	}

	return status;
}


/*
 * FileRepPrimary_ReconcileXLogEof
 */
int
FileRepPrimary_ReconcileXLogEof(
								FileRepIdentifier_u fileRepIdentifier,
								FileRepRelationType_e fileRepRelationType,
								XLogRecPtr primaryXLogEof)
{

	int			status = STATUS_OK;

	FileRepOperationDescription_u descr;

	descr.reconcile.xLogEof = primaryXLogEof;

	if (FileRepPrimary_IsMirroringRequired(fileRepRelationType, FileRepOperationReconcileXLogEof))
	{
		status = FileRepPrimary_ConstructAndInsertMessage(
														  fileRepIdentifier,
														  fileRepRelationType,
														  FileRepOperationReconcileXLogEof,
														  descr,
														  NULL, /* data */
														  0);	/* length */
	}

	return status;
}

/*
 * FileRepPrimary_Rename
 */
int
FileRepPrimary_MirrorRename(
							FileRepIdentifier_u oldFileRepIdentifier,
							FileRepIdentifier_u newFileRepIdentifier,
							FileRepRelationType_e fileRepRelationType)
{

	int			status = STATUS_OK;

	FileRepOperationDescription_u descr;

	descr.rename.fileRepIdentifier = newFileRepIdentifier;

	if (FileRepPrimary_IsMirroringRequired(fileRepRelationType, FileRepOperationRename))
	{
		if (!(dataState == DataStateInResync &&
			  fileRepRelationType == FileRepRelationTypeFlatFile &&
			  segmentState != SegmentStateInSyncTransition))
		{
			status = FileRepPrimary_ConstructAndInsertMessage(
															  oldFileRepIdentifier,
															  fileRepRelationType,
															  FileRepOperationRename,
															  descr,
															  NULL, /* data */
															  0);	/* data length */
		}
	}

	return status;
}


/*
 * FileRepPrimary_MirrorWrite
 */
int
FileRepPrimary_MirrorWrite(FileRepIdentifier_u fileRepIdentifier,
						   FileRepRelationType_e fileRepRelationType,
						   int32 offset,
						   char *data,
						   uint32 dataLength,
						   XLogRecPtr lsn)
{
	FileRepShmemMessageDescr_s *fileRepShmemMessageDescr;
	FileRepMessageHeader_s *fileRepMessageHeader;
	pg_crc32   *fileRepMessageHeaderCrc;
	pg_crc32	fileRepMessageHeaderCrcLocal;
	char	   *fileRepMessageBody;
	char	   *msgPositionInsert = NULL;
	uint32		msgLength;
	uint32		dataLengthLocal;
	int			status = STATUS_OK;
	uint32		spareField;

	if (FileRepPrimary_IsMirroringRequired(fileRepRelationType, FileRepOperationWrite))
	{
		if (!(dataState == DataStateInResync &&
			  fileRepRelationType == FileRepRelationTypeFlatFile &&
			  segmentState != SegmentStateInSyncTransition))
		{

			/*
			 * Prevent cancel/die interrupt while doing this multi-step in
			 * order to insert message and update message state atomically
			 * with respect to cancel/die interrupts. See MPP-10040.
			 */

			HOLD_INTERRUPTS();

			while (dataLength > 0)
			{

				dataLengthLocal = Min(FILEREP_MESSAGEBODY_LEN, dataLength);

				msgLength = sizeof(FileRepMessageHeader_s) + sizeof(pg_crc32) + dataLengthLocal;

				/*
				 * Shared memory is reserved in advance in order to avoid
				 * holding lock during memcpy() and Crc calculation...
				 *
				 * Construct FileRepShmemMessageDescr_s
				 */
				msgPositionInsert = FileRep_ReserveShmem(
														 fileRepShmemArray[fileRepProcIndex],
														 msgLength,
														 &spareField,
														 FileRepOperationWrite,
														 FileRepShmemLock);

				if (msgPositionInsert == NULL)
				{

					status = STATUS_ERROR;

					if ((!primaryMirrorIsIOSuspended()) && dataState != DataStateInChangeTracking)
					{
						ereport(WARNING,
								(errmsg("mirror failure, "
										"could not queue message to be mirrored, "
										"failover requested"),
								 errhint("run gprecoverseg to re-establish mirror connectivity"),
								 FileRep_errdetail(fileRepIdentifier,
												   fileRepRelationType,
												   FileRepOperationWrite,
												   FILEREP_UNDEFINED),
								 FileRep_errdetail_Shmem(),
								 FileRep_errcontext()));

						FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
						FileRepSubProcess_ProcessSignals();
					}

					FileRepPrimary_IsMirroringRequired(fileRepRelationType, FileRepOperationWrite);

					break;
				}

				/* Construct FileRepMessageHeader_s */

				fileRepMessageHeader =
					(FileRepMessageHeader_s *) (msgPositionInsert +
												sizeof(FileRepShmemMessageDescr_s));

				fileRepMessageHeader->fileRepMessageHeaderVersion = FileRepMessageHeaderVersionOne;

				fileRepMessageHeader->fileRepRelationType = fileRepRelationType;

				fileRepMessageHeader->fileRepOperation = FileRepOperationWrite;

				fileRepMessageHeader->fileRepIdentifier = fileRepIdentifier;

				fileRepMessageHeader->messageCount = spareField;

				fileRepMessageHeader->fileRepOperationDescription.write.offset = offset;
				fileRepMessageHeader->fileRepOperationDescription.write.dataLength = dataLengthLocal;
				fileRepMessageHeader->fileRepOperationDescription.write.lsn = lsn;
				fileRepMessageHeader->messageBodyLength = dataLengthLocal;

				/* Construct fileRepMessageBodyCrc */
				FileRep_CalculateCrc((char *) data,
									 dataLengthLocal,
									 &fileRepMessageHeader->fileRepMessageBodyCrc);

				/*
				 * The following fields are not in use during open(). They
				 * were zeroed during reserving message buffer. a)
				 * fileRepOperationDescription c) messageCount
				 */

				/* Construct fileRepMessageHeaderCrc */

				fileRepMessageHeaderCrc =
					(pg_crc32 *) (msgPositionInsert +
								  sizeof(FileRepMessageHeader_s) +
								  sizeof(FileRepShmemMessageDescr_s));

				FileRep_CalculateCrc((char *) fileRepMessageHeader,
									 sizeof(FileRepMessageHeader_s),
									 &fileRepMessageHeaderCrcLocal);

				*fileRepMessageHeaderCrc = fileRepMessageHeaderCrcLocal;

				FileRep_InsertLogEntry(
									   "P_ConstructAndInsertMessage",
									   fileRepIdentifier,
									   fileRepRelationType,
									   FileRepOperationWrite,
									   fileRepMessageHeaderCrcLocal,
									   fileRepMessageHeader->fileRepMessageBodyCrc,
									   FileRepAckStateNotInitialized,
									   FILEREP_UNDEFINED,
									   fileRepMessageHeader->messageCount);

				/* Copy Data */
				fileRepMessageBody = (char *) (msgPositionInsert +
											   sizeof(FileRepMessageHeader_s) +
											   sizeof(FileRepShmemMessageDescr_s) +
											   sizeof(pg_crc32));

				memcpy(fileRepMessageBody, data, dataLengthLocal);

				fileRepShmemMessageDescr =
					(FileRepShmemMessageDescr_s *) msgPositionInsert;

				fileRepShmemMessageDescr->messageSync =
					FileRep_IsOperationSynchronous(FileRepOperationWrite);

				fileRepShmemMessageDescr->messageState = FileRepShmemMessageStateReady;

				LWLockAcquire(FileRepShmemLock, LW_EXCLUSIVE);
				FileRep_IpcSignal(fileRepIpcArray[fileRepShmemArray[fileRepProcIndex]->ipcArrayIndex]->semC,
								  &fileRepIpcArray[fileRepShmemArray[fileRepProcIndex]->ipcArrayIndex]->refCountSemC);
				LWLockRelease(FileRepShmemLock);

				/* the order should not be changed. */
				data = (char *) data + dataLengthLocal;
				if (offset != FILEREP_OFFSET_UNDEFINED)
				{
					offset += dataLengthLocal;
				}
				dataLength -= dataLengthLocal;

			} //while ()

				RESUME_INTERRUPTS();
		} //if
	}

	return
		status;
}

/*
 * FileRepPrimary_MirrorCreate
 */
int
FileRepPrimary_MirrorCreate(FileRepIdentifier_u fileRepIdentifier,
							FileRepRelationType_e fileRepRelationType,
							bool ignoreAlreadyExists)
{
	int			status = STATUS_OK;

	FileRepOperationDescription_u descr;

	descr.create.ignoreAlreadyExists = ignoreAlreadyExists;

	if (FileRepPrimary_IsMirroringRequired(fileRepRelationType, FileRepOperationCreate))
	{
		if (!(dataState == DataStateInResync &&
			  fileRepRelationType == FileRepRelationTypeFlatFile &&
			  segmentState != SegmentStateInSyncTransition))
		{
			status = FileRepPrimary_ConstructAndInsertMessage(
															  fileRepIdentifier,
															  fileRepRelationType,
															  FileRepOperationCreate,
															  descr,
															  NULL, /* data */
															  0);	/* data length */
		}
	}

	return status;
}

/*
 * FileRepPrimary_MirrorDrop
 */
int
FileRepPrimary_MirrorDrop(FileRepIdentifier_u fileRepIdentifier,
						  FileRepRelationType_e fileRepRelationType)
{
	int			status = STATUS_OK;

	FileRepOperationDescription_u descr;

	if (FileRepPrimary_IsMirroringRequired(fileRepRelationType, FileRepOperationDrop))
	{
		if (!(dataState == DataStateInResync &&
			  fileRepRelationType == FileRepRelationTypeFlatFile &&
			  segmentState != SegmentStateInSyncTransition))
		{
			/* Just to make compiler happy */
			descr.defaultDesc.placeholder = false;

			status = FileRepPrimary_ConstructAndInsertMessage(
															  fileRepIdentifier,
															  fileRepRelationType,
															  FileRepOperationDrop,
															  descr,
															  NULL, /* data */
															  0);	/* data length */
		}
	}

	return status;
}

/*
 * FileRepPrimary_MirrorDropFileFromDir
 */
int
FileRepPrimary_MirrorDropFilesFromDir(FileRepIdentifier_u fileRepIdentifier,
									  FileRepRelationType_e fileRepRelationType)
{
	int			status = STATUS_OK;

	FileRepOperationDescription_u descr;

	if (FileRepPrimary_IsMirroringRequired(fileRepRelationType, FileRepOperationDropFilesFromDir))
	{
		/* Just to make compiler happy */
		descr.defaultDesc.placeholder = false;

		status = FileRepPrimary_ConstructAndInsertMessage(
														  fileRepIdentifier,
														  fileRepRelationType,
														  FileRepOperationDropFilesFromDir,
														  descr,
														  NULL, /* data */
														  0);	/* data length */
	}

	return status;
}

/*
 * FileRepPrimary_MirrorDropTemporaryFiles
 */
int
FileRepPrimary_MirrorDropTemporaryFiles(
										FileRepIdentifier_u fileRepIdentifier,
										FileRepRelationType_e fileRepRelationType)
{
	int			status = STATUS_OK;

	FileRepOperationDescription_u descr;

	if (FileRepPrimary_IsMirroringRequired(fileRepRelationType, FileRepOperationDropTemporaryFiles))
	{
		/* Just to make compiler happy */
		descr.defaultDesc.placeholder = false;

		status = FileRepPrimary_ConstructAndInsertMessage(
														  fileRepIdentifier,
														  fileRepRelationType,
														  FileRepOperationDropTemporaryFiles,
														  descr,
														  NULL, /* data */
														  0);	/* data length */
	}

	return status;
}

/*
 * FileRepPrimary_MirrorValidation
 */
int
FileRepPrimary_MirrorValidation(FileRepIdentifier_u fileRepIdentifier,
								FileRepRelationType_e fileRepRelationType,
								FileRepValidationType_e fileRepValidationType)
{
	int			status = STATUS_OK;
	int			mirrorStatus = FileRepStatusSuccess;

	FileRepOperationDescription_u descr;

	descr.validation.fileRepValidationType = fileRepValidationType;

	if (FileRepPrimary_IsMirroringRequired(fileRepRelationType, FileRepOperationValidation))
	{
		status = FileRepPrimary_ConstructAndInsertMessage(
														  fileRepIdentifier,
														  fileRepRelationType,
														  FileRepOperationValidation,
														  descr,
														  NULL, /* data */
														  0);	/* data length */
		if (status != STATUS_OK)
		{
			mirrorStatus = FileRepStatusMirrorLossOccurred;
			return mirrorStatus;
		}

		FileRepAckPrimary_IsOperationCompleted(
											   fileRepIdentifier,
											   fileRepRelationType);

		mirrorStatus = FileRepAckPrimary_GetMirrorErrno();

		/*
		 * During resynchronization with incremental copy if directories and
		 * relations marked with MirroredObjectExistenceState_MirrorCreated do
		 * not exist on mirror then Fault is reported and resynchronization
		 * with full copy requested.
		 */
		if (fileRepValidationType == FileRepValidationExistence &&
			mirrorStatus == FileRepStatusNoSuchFileOrDirectory)
		{
			ereport(WARNING,
					(errmsg("mirror failure, "
							"could not resynchronize mirror due to missing directories or relations on mirror, "
							"failover requested"),
					 errhint("run gprecoverseg -F (full copy) to re-establish mirror connectivity"),
					 errSendAlert(true),
					 FileRep_errdetail(fileRepIdentifier,
									   fileRepRelationType,
									   FileRepOperationValidation,
									   FILEREP_UNDEFINED),
					 FileRep_errcontext()));

			FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
			FileRepSubProcess_ProcessSignals();
		}
	}

	return mirrorStatus;
}

/*
 * FileRepPrimary_MirrorShutdown
 *	inform mirror about graceful shutdown
 */
void
FileRepPrimary_MirrorShutdown(void)
{
	int			status = STATUS_OK;
	bool		retval = TRUE;
	FileRepIdentifier_u fileRepIdentifier;
	FileRepOperationDescription_u descr;

	fileRepIdentifier = FileRep_GetUnknownIdentifier("shutdown");

	/* see if we should send the shutdown to the mirror */
	bool		shouldSendShutdownToMirror = false;

	getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);
	if (dataState == DataStateInSync)
	{
		shouldSendShutdownToMirror = true;
	}
	else if (dataState == DataStateInResync &&
			 segmentState != SegmentStateInitialization &&
			 segmentState != SegmentStateInResyncTransition)
	{
		shouldSendShutdownToMirror = true;
	}

	/* send graceful shutdown if required */
	if (shouldSendShutdownToMirror)
	{
		/* Just to make compiler happy */
		descr.defaultDesc.placeholder = false;

		status = FileRepPrimary_ConstructAndInsertMessage(
														  fileRepIdentifier,
														  FileRepRelationTypeUnknown,
														  FileRepOperationShutdown,
														  descr,
														  NULL, /* data */
														  0);	/* data length */
		if (status != STATUS_OK)
			return;
		retval = FileRepAckPrimary_IsOperationCompleted(
														fileRepIdentifier,
														FileRepRelationTypeUnknown);

		if (retval == FALSE &&
			dataState != DataStateInChangeTracking &&
			!primaryMirrorIsIOSuspended())
		{
			ereport(WARNING,
					(errmsg("mirror failure, "
							"could not complete operation on mirror, "
							"failover requested"),
					 errhint("run gprecoverseg to re-establish mirror connectivity"),
					 FileRep_errdetail(fileRepIdentifier,
									   FileRepRelationTypeUnknown,
									   FileRepOperationShutdown,
									   FILEREP_UNDEFINED),
					 FileRep_errdetail_Shmem(),
					 FileRep_errdetail_ShmemAck(),
					 FileRep_errcontext()));

			FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
			FileRepSubProcess_ProcessSignals();
		}
	}

	return;
}

/*
 * FileRepPrimary_MirrorInSyncTransition
 * transition mirror from InResync to InSync dataState
 */
void
FileRepPrimary_MirrorInSyncTransition(void)
{
	int			status = STATUS_OK;
	bool		retval = TRUE;
	FileRepIdentifier_u fileRepIdentifier;
	FileRepOperationDescription_u descr;

	fileRepIdentifier = FileRep_GetUnknownIdentifier("inSyncTransition");

	if (FileRepPrimary_IsMirroringRequired(FileRepRelationTypeUnknown, FileRepOperationInSyncTransition))
	{
		/* Just to make compiler happy */
		descr.defaultDesc.placeholder = false;

		status = FileRepPrimary_ConstructAndInsertMessage(
														  fileRepIdentifier,
														  FileRepRelationTypeUnknown,
														  FileRepOperationInSyncTransition,
														  descr,
														  NULL, /* data */
														  0);	/* data length */
		if (status != STATUS_OK)
			return;

		retval = FileRepAckPrimary_IsOperationCompleted(
														fileRepIdentifier,
														FileRepRelationTypeUnknown);
		if (retval == FALSE &&
			dataState != DataStateInChangeTracking &&
			!primaryMirrorIsIOSuspended())
		{
			ereport(WARNING,
					(errmsg("mirror failure, "
							"could not complete operation on mirror, "
							"failover requested"),
					 errhint("run gprecoverseg to re-establish mirror connectivity"),
					 FileRep_errdetail(fileRepIdentifier,
									   FileRepRelationTypeUnknown,
									   FileRepOperationInSyncTransition,
									   FILEREP_UNDEFINED),
					 FileRep_errdetail_Shmem(),
					 FileRep_errdetail_ShmemAck(),
					 FileRep_errcontext()));

			FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
			FileRepSubProcess_ProcessSignals();
		}

	}

	return;
}

/*
 * FileRepPrimary_MirrorHeartBeat
 * verify that flow from primary to mirror and back is alive
 */
void
FileRepPrimary_MirrorHeartBeat(FileRepConsumerProcIndex_e index)
{
	int			status = STATUS_OK;
	bool		retval = TRUE;
	FileRepIdentifier_u fileRepIdentifier;
	FileRepOperationDescription_u descr;

	descr.heartBeat.fileRepConsumerProcIndex = index;

	fileRepIdentifier = FileRep_GetUnknownIdentifier("heartBeat");


	if (FileRepPrimary_IsMirroringRequired(FileRepRelationTypeUnknown, FileRepOperationHeartBeat))
	{
		status = FileRepPrimary_ConstructAndInsertMessage(
														  fileRepIdentifier,
														  FileRepRelationTypeUnknown,
														  FileRepOperationHeartBeat,
														  descr,
														  NULL, /* data */
														  0);	/* data length */

		if (status != STATUS_OK)
			return;

		retval = FileRepAckPrimary_IsOperationCompleted(
														fileRepIdentifier,
														FileRepRelationTypeUnknown);
		if (retval == FALSE &&
			dataState != DataStateInChangeTracking &&
			!primaryMirrorIsIOSuspended())
		{
			ereport(WARNING,
					(errmsg("mirror failure, "
							"could not complete operation on mirror, "
							"failover requested"),
					 errhint("run gprecoverseg to re-establish mirror connectivity"),
					 FileRep_errdetail(fileRepIdentifier,
									   FileRepRelationTypeUnknown,
									   FileRepOperationHeartBeat,
									   FILEREP_UNDEFINED),
					 FileRep_errdetail_Shmem(),
					 FileRep_errdetail_ShmemAck(),
					 FileRep_errcontext()));

			FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
			FileRepSubProcess_ProcessSignals();
		}
	}

	return;
}

bool
FileRepPrimary_IsOperationCompleted(
									FileRepIdentifier_u fileRepIdentifier,
									FileRepRelationType_e fileRepRelationType)
{
	bool		retval = TRUE;

	if (FileRepPrimary_IsMirroringRequired(fileRepRelationType, FileRepOperationNotSpecified))
	{
		if (!(dataState == DataStateInResync &&
			  fileRepRelationType == FileRepRelationTypeFlatFile &&
			  segmentState != SegmentStateInSyncTransition))
		{
			retval = FileRepAckPrimary_IsOperationCompleted(
															fileRepIdentifier,
															fileRepRelationType);

			/* suspend if mirror reported fault */
			if (retval == FALSE)
			{
				if (dataState != DataStateInChangeTracking && !primaryMirrorIsIOSuspended())
				{
					ereport(WARNING,
							(errmsg("mirror failure, "
									"could not complete operation on mirror, "
									"failover requested"),
							 errhint("run gprecoverseg to re-establish mirror connectivity"),
							 FileRep_errdetail(fileRepIdentifier,
											   fileRepRelationType,
											   FileRepOperationNotSpecified,
											   FILEREP_UNDEFINED),
							 FileRep_errdetail_Shmem(),
							 FileRep_errdetail_ShmemAck(),
							 FileRep_errcontext()));

					FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
					FileRepSubProcess_ProcessSignals();
				}
				FileRepPrimary_IsMirroringRequired(fileRepRelationType,
												   FileRepOperationNotSpecified);
			}
		}
	}

	/*
	 * If fault occured on mirror and segment state is shutDown, with
	 * dataState DataStateInSync or DataStateInResync best to ERROR here
	 * itself. If returned from here primary goes ahead to performs IO only on
	 * primary. This also covers for case where inserting message for ACK
	 * itself had failed, as if entry doesn't exist
	 * FileRepAckPrimary_IsOperationCompleted returns FALSE. Case for non
	 * filerep backends is covered in FileRepPrimary_IsMirroringRequired,
	 * hence skipping the same here.
	 */
	if (fileRepRole == FileRepPrimaryRole &&
		(dataState == DataStateInResync || dataState == DataStateInSync) &&
		FileRepIsBackendSubProcess(fileRepProcessType) &&
		(segmentState == SegmentStateImmediateShutdown ||
		 segmentState == SegmentStateShutdown ||
		 segmentState == SegmentStateShutdownFilerepBackends) &&
		fileRepShmemArray[0]->state == FileRepStateFault)
	{
		ereport(LOG,
				(errmsg("failure detected in segment mirroring during shutdown, "
						"mirroring role '%s' "
						"mirroring state '%s' "
						"segment state '%s' "
						"filerep state '%s' "
						"process name(pid) '%s(%d)' ",
						FileRepRoleToString[fileRepRole],
						DataStateToString[dataState],
						SegmentStateToString[segmentState],
						FileRepStateToString[FileRepSubProcess_GetState()],
						FileRepProcessTypeToString[fileRepProcessType],
						getpid())));

		elog(ERROR,
			 "failure detected in segment mirroring during shutdown relation type %s",
			 FileRepRelationTypeToString[fileRepRelationType]);
	}

	return retval;
}

XLogRecPtr
FileRepPrimary_GetMirrorXLogEof(void)
{
	return FileRepAckPrimary_GetMirrorXLogEof();
}

int
FileRepPrimary_GetMirrorStatus(void)
{
	return FileRepAckPrimary_GetMirrorErrno();
}

/****************************************************************
 * FILEREP SUB-PROCESS (FileRep Primary SENDER Process)
 ****************************************************************/
/*
 * Primary Sender process is responsible for
 *
 *		b) Data Messages
 *				*) data written by backend
 *
 */


/*
 *
 * FileRepPrimary_StartSender
 * check hash_create for allocating memory context
 */
void
FileRepPrimary_StartSender(void)
{
	int			status = STATUS_OK;
	int			retry = 0;
	struct timeval currentTime;
	pg_time_t	beginTime = 0;
	pg_time_t	endTime = 0;

	FileRep_InsertConfigLogEntry("start sender");

	{
		char		tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];

		snprintf(tmpBuf, sizeof(tmpBuf), "primary address(port) '%s(%d)' mirror address(port) '%s(%d)' ",
				 fileRepPrimaryHostAddress,
				 fileRepPrimaryPort,
				 fileRepMirrorHostAddress,
				 fileRepMirrorPort);

		FileRep_InsertConfigLogEntry(tmpBuf);
	}

	while (1)
	{

		if (status != STATUS_OK)
		{
			FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
			FileRepSubProcess_SetState(FileRepStateFault);
		}

		while (FileRepSubProcess_GetState() == FileRepStateFault)
		{

			FileRepSubProcess_ProcessSignals();
			pg_usleep(50000L);	/* 50 ms */
		}

		if (FileRepSubProcess_GetState() == FileRepStateShutdown)
		{

			break;
		}

		Assert(FileRepSubProcess_GetState() == FileRepStateInitialization);

		Insist(fileRepRole == FileRepPrimaryRole);
		Insist(dataState == DataStateInSync ||
			   dataState == DataStateInResync);

		status = FileRepConnClient_EstablishConnection(
													   fileRepMirrorHostAddress,
													   fileRepMirrorPort,
													   FALSE /* reportError */ );

		if (status != STATUS_OK)
		{
			gettimeofday(&currentTime, NULL);
			beginTime = (pg_time_t) currentTime.tv_sec;
		}

		while (status != STATUS_OK &&
			   FileRep_IsRetry(retry) &&
			   (endTime - beginTime) < gp_segment_connect_timeout)
		{
			FileRep_Sleep10ms(retry);

			FileRep_IncrementRetry(retry);

			gettimeofday(&currentTime, NULL);
			endTime = (pg_time_t) currentTime.tv_sec;

			status = FileRepConnClient_EstablishConnection(
														   fileRepMirrorHostAddress,
														   fileRepMirrorPort,
														   (retry == file_rep_retry && file_rep_retry != 0) ||
														   ((endTime - beginTime) > gp_segment_connect_timeout) ? TRUE : FALSE);

			if (FileRepSubProcess_IsStateTransitionRequested())
			{
				break;
			}
		}

		if (status != STATUS_OK)
		{
			continue;
		}

		FileRep_SetFileRepRetry();

		status = FileRepPrimary_RunSender();

	} //while (1)

		FileRepConnClient_CloseConnection();

	return;
}

/*
 * SenderLoop
 *
 */
static int
FileRepPrimary_RunSender(void)
{
	FileRepShmemMessageDescr_s *fileRepShmemMessageDescr = NULL;
	char	   *fileRepMessage;
	int			status = STATUS_OK;
	bool		movePositionConsume = FALSE;
	uint32		spare = 0;
	FileRepMessageHeader_s *fileRepMessageHeader;
	FileRepShmem_s *fileRepShmem = fileRepShmemArray[fileRepProcIndex];
	FileRepConsumerProcIndex_e messageType = FileRepMessageTypeUndefined;

	FileRep_InsertConfigLogEntry("run sender");

	while (1)
	{

		LWLockAcquire(FileRepShmemLock, LW_EXCLUSIVE);

		if (movePositionConsume)
		{

			fileRepShmem->positionConsume =
				fileRepShmem->positionConsume +
				fileRepShmemMessageDescr->messageLength +
				sizeof(FileRepShmemMessageDescr_s);

			if (fileRepShmem->positionConsume == fileRepShmem->positionWraparound &&
				fileRepShmem->positionInsert != fileRepShmem->positionWraparound)
			{

				fileRepShmem->positionConsume = fileRepShmem->positionBegin;
				fileRepShmem->positionWraparound = fileRepShmem->positionEnd;
			}

			FileRep_IpcSignal(fileRepIpcArray[fileRepShmem->ipcArrayIndex]->semP,
							  &fileRepIpcArray[fileRepShmem->ipcArrayIndex]->refCountSemP);
		}

		fileRepShmemMessageDescr =
			(FileRepShmemMessageDescr_s *) fileRepShmem->positionConsume;

		while ((fileRepShmem->positionConsume == fileRepShmem->positionInsert) ||
			   ((fileRepShmem->positionConsume != fileRepShmem->positionInsert) &&
				(fileRepShmemMessageDescr->messageState != FileRepShmemMessageStateReady)))
		{

			fileRepIpcArray[fileRepShmem->ipcArrayIndex]->refCountSemC++;

			LWLockRelease(FileRepShmemLock);

			FileRepSubProcess_ProcessSignals();
			if (FileRepSubProcess_GetState() != FileRepStateReady &&
				FileRepSubProcess_GetState() != FileRepStateInitialization)
			{

				LWLockAcquire(FileRepShmemLock, LW_EXCLUSIVE);
				break;
			}

			FileRep_IpcWait(fileRepIpcArray[fileRepShmem->ipcArrayIndex]->semC, &fileRepIpcArray[fileRepShmem->ipcArrayIndex]->refCountSemC, FileRepShmemLock);

			LWLockAcquire(FileRepShmemLock, LW_EXCLUSIVE);

			if (fileRepShmem->positionConsume == fileRepShmem->positionWraparound &&
				fileRepShmem->positionInsert != fileRepShmem->positionWraparound)
			{

				fileRepShmem->positionConsume = fileRepShmem->positionBegin;
				fileRepShmem->positionWraparound = fileRepShmem->positionEnd;
			}

			/* Re-assign to find if messageState is changed */
			fileRepShmemMessageDescr =
				(FileRepShmemMessageDescr_s *) fileRepShmem->positionConsume;
		}
		fileRepShmem->consumeCount++;

		LWLockRelease(FileRepShmemLock);

		FileRepSubProcess_ProcessSignals();
		if (FileRepSubProcess_GetState() != FileRepStateReady &&
			FileRepSubProcess_GetState() != FileRepStateInitialization)
		{
			break;
		}

		SIMPLE_FAULT_INJECTOR(FileRepSender);

		fileRepMessage = (char *) (fileRepShmem->positionConsume +
								   sizeof(FileRepShmemMessageDescr_s));


		fileRepMessageHeader = (FileRepMessageHeader_s *) (fileRepShmem->positionConsume +
														   sizeof(FileRepShmemMessageDescr_s));

		FileRep_InsertLogEntry(
							   "P_RunSender",
							   fileRepMessageHeader->fileRepIdentifier,
							   fileRepMessageHeader->fileRepRelationType,
							   fileRepMessageHeader->fileRepOperation,
							   FILEREP_UNDEFINED,
							   fileRepMessageHeader->fileRepMessageBodyCrc,
							   FileRepAckStateNotInitialized,
							   spare,
							   fileRepMessageHeader->messageCount);

		spare = fileRepMessageHeader->messageCount;

		switch (fileRepMessageHeader->fileRepRelationType)
		{
			case FileRepRelationTypeUnknown:
				switch (fileRepMessageHeader->fileRepOperation)
				{
					case FileRepOperationShutdown:
						messageType = FileRepMessageTypeShutdown;
						break;

					case FileRepOperationInSyncTransition:
						messageType = FileRepMessageTypeXLog;
						break;

					case FileRepOperationHeartBeat:
						messageType = fileRepMessageHeader->fileRepOperationDescription.heartBeat.fileRepConsumerProcIndex;
						break;

					default:
						Assert(0);
						break;
				}
				break;
			case FileRepRelationTypeFlatFile:
			case FileRepRelationTypeBulk:
				messageType = FileRepMessageTypeXLog;
				break;

			case FileRepRelationTypeAppendOnly:
				messageType = FileRepMessageTypeAO01;
				break;

			case FileRepRelationTypeBufferPool:
			case FileRepRelationTypeDir:
				messageType = FileRepMessageTypeWriter;
				break;

			default:
				ereport(WARNING,
						(errmsg("mirror failure, "
								"unknown relation type '%d' "
								"failover requested",
								fileRepMessageHeader->fileRepRelationType),
						 errhint("run gprecoverseg to re-establish mirror connectivity"),
						 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
										   fileRepMessageHeader->fileRepRelationType,
										   fileRepMessageHeader->fileRepOperation,
										   fileRepMessageHeader->messageCount),
						 FileRep_errcontext()));

				status = STATUS_ERROR;
				break;
		}

		if (!FileRepConnClient_SendMessage(
										   messageType,
										   fileRepShmemMessageDescr->messageSync,
										   fileRepMessage,
										   fileRepShmemMessageDescr->messageLength))
		{

			if (!primaryMirrorIsIOSuspended())
			{
				ereport(WARNING,
						(errcode_for_socket_access(),
						 errmsg("mirror failure, "
								"could not sent message to mirror msg header count '%d' local count '%d' : %m, "
								"failover requested",
								fileRepMessageHeader->messageCount,
								spare),
						 errhint("run gprecoverseg to re-establish mirror connectivity"),
						 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
										   fileRepMessageHeader->fileRepRelationType,
										   fileRepMessageHeader->fileRepOperation,
										   fileRepMessageHeader->messageCount),
						 FileRep_errdetail_Shmem(),
						 FileRep_errcontext()));
			}
			status = STATUS_ERROR;
			break;
		}

		movePositionConsume = TRUE;
	} //while (1)

		FileRepConnClient_CloseConnection();

	return status;
}


/*
 * Send the message to the mirror to start computation of the checksum file.
 */
int
FileRepPrimary_MirrorStartChecksum(FileRepIdentifier_u fileRepIdentifier)
{
	int			status = STATUS_OK;

	FileRepOperationDescription_u descr;

	if (FileRepPrimary_IsMirroringRequired(FileRepRelationTypeFlatFile,
										   FileRepOperationStartSlruChecksum))
	{
		descr.startChecksum.mirrorStatus = FileRepStatusSuccess;

		status = FileRepPrimary_ConstructAndInsertMessage(fileRepIdentifier,
														  FileRepRelationTypeFlatFile,
														  FileRepOperationStartSlruChecksum,
														  descr,
														  NULL, /* data */
														  0);	/* data length */
		if (status)
			return status;

	}
	else
		ereport(LOG, (errmsg("SLRU checksum start bypassed as mirroring is not required")));

	return status;
}

/*
 * Send a message to the mirror to compare the primary's directory checksum with
 * the mirror's.
 */
int
FileRepPrimary_MirrorVerifyDirectoryChecksum(
											 FileRepIdentifier_u fileRepIdentifier, char *md5)
{
	int			status = STATUS_OK;
	int			mirrorStatus = FileRepStatusSuccess;

	FileRepOperationDescription_u descr;

	if (FileRepPrimary_IsMirroringRequired(FileRepRelationTypeFlatFile,
										   FileRepOperationVerifySlruDirectoryChecksum))
	{
		memcpy(descr.verifyDirectoryChecksum.md5, md5, SLRU_MD5_BUFLEN);
		descr.verifyDirectoryChecksum.mirrorStatus = FileRepStatusSuccess;

		status = FileRepPrimary_ConstructAndInsertMessage(fileRepIdentifier,
														  FileRepRelationTypeFlatFile,
														  FileRepOperationVerifySlruDirectoryChecksum,
														  descr,
														  NULL, /* data */
														  0);	/* data length */
		if (status)
			return status;

		FileRepAckPrimary_IsOperationCompleted(fileRepIdentifier, FileRepRelationTypeFlatFile);

		mirrorStatus = FileRepAckPrimary_GetMirrorErrno();

		if (FileRepStatusSuccess != mirrorStatus)
		{
			ereport(WARNING,
					(errmsg("SLRU checksums did not match, mirrorStatus = '%s', directory = '%s'",
							FileRepStatusToString[mirrorStatus],
							fileRepIdentifier.fileRepFlatFileIdentifier.directorySimpleName)));
			return STATUS_ERROR;
		}
	}
	else
		ereport(LOG, (errmsg("SLRU checksum validation bypassed as mirroring is not required")));

	return status;
}
