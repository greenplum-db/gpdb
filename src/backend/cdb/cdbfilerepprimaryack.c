/*-------------------------------------------------------------------------
 *
 * cdbfilerepprimaryack.c
 *
 * Portions Copyright (c) 2009-2010 Greenplum Inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbfilerepprimaryack.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <signal.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "cdb/cdbvars.h"
#include "cdb/cdbfilerepservice.h"
#include "cdb/cdbfilerepprimaryack.h"
#include "cdb/cdbfilerep.h"
#include "cdb/cdbfilerepconnserver.h"
#include "libpq/pqsignal.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/shmem.h"
#include "utils/faultinjector.h"
#include "utils/hsearch.h"
#include "utils/ps_status.h"
#include "utils/memutils.h"


/*
 * It specifies fields maintained by each entry in hash table
 * of acks.
 *
 * Only SYNC messages are entered into hash table
 * NOTE Do we need to maintain caller? For TXs that shares the same file
 * taht will be required.
 */
typedef struct FileRepAckHashEntry_s
{
	char		fileName[MAXPGPATH + 1];
	/* It is key. It has to be first field in the structure. */

	FileRepOperation_e fileRepOperation;

	/*
	 * File operation that is acknowledged. It is required for consistency
	 * check.
	 */

	FileRepRelationType_e fileRepRelationType;
	/* Type of the file. It is required for consistency check. */

	FileRepAckState_e fileRepAckState;
	/* state of the ack */

	XLogRecPtr	xLogEof;
	/* logical xlog EOF, required for xlog reconcile */

	int			mirrorStatus;

} FileRepAckHashEntry_s;

typedef struct FileRepAckHashShmem_s
{

	int			ipcArrayIndex;

	HTAB	   *hash;

} FileRepAckHashShmem_s;

static FileRepAckHashShmem_s *fileRepAckHashShmem = NULL;

static void FileRepAckPrimary_ShmemReInit(void);

static XLogRecPtr xLogEof = {0, 0};

static int	mirrorStatus = FileRepStatusSuccess;

/*
 * Find if acknowledgement is received from mirror.
 * Access is protected by shared  lock.
 */
static FileRepAckHashEntry_s *FileRepAckPrimary_LookupHashEntry(FileName fileName);

/*
 * Insert entry with info related to ack in hash table.
 * Access is protected by exclusive lock.
 */
static FileRepAckHashEntry_s *FileRepAckPrimary_InsertHashEntry(
								  FileName fileName,
								  bool *exists);

/*
 * Remove entry from hash table.
 * Return TRUE if ack was removed.
 * Access is protected by exclusive lock.
 */
static bool FileRepAckPrimary_RemoveHashEntry(FileName fileName);


static int FileRepAckPrimary_UpdateHashEntry(
								  FileRepIdentifier_u fileRepIdentifier,
								  FileRepRelationType_e fileRepRelationType,
								  FileRepAckState_e fileRepAckState);

static int	FileRepAckPrimary_RunReceiver(void);

static int	FileRepAckPrimary_RunConsumer(void);


/****************************************************************
 * FILEREP SUB-PROCESS (FileRep Ack Primary RECEIVER Process)
 ****************************************************************/
/*
 *
 * FileRepAckPrimary_StartReceiver
 */
void
FileRepAckPrimary_StartReceiver(void)
{
	int			status = STATUS_OK;
	struct timeval currentTime;
	pg_time_t	beginTime = 0;
	pg_time_t	endTime = 0;
	int			retval = 0;

	FileRep_InsertConfigLogEntry("start receiver ack");

	{
		char		tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];

		snprintf(tmpBuf, sizeof(tmpBuf), "primary address(port) '%s(%d)' mirror address(port) '%s(%d)' ",
				 fileRepPrimaryHostAddress,
				 fileRepPrimaryPort,
				 fileRepMirrorHostAddress,
				 fileRepMirrorPort);

		FileRep_InsertConfigLogEntry(tmpBuf);
	}

	FileRepAckPrimary_ShmemReInit();

	Insist(fileRepRole == FileRepPrimaryRole);

	status = FileRepConnServer_StartListener(
											 fileRepPrimaryHostAddress,
											 fileRepPrimaryPort);

	gettimeofday(&currentTime, NULL);
	beginTime = (pg_time_t) currentTime.tv_sec;

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

		PG_SETMASK(&BlockSig);
		retval = FileRepConnServer_Select();
		PG_SETMASK(&UnBlockSig);

		gettimeofday(&currentTime, NULL);
		endTime = (pg_time_t) currentTime.tv_sec;

		if ((endTime - beginTime) > gp_segment_connect_timeout)
		{
			ereport(WARNING,
					(errmsg("mirror failure, "
							"no connection was established from client from mirror, "
							"primary address(port) '%s(%d)' mirror address(port) '%s(%d)' timeout reached '%d' "
							"failover requested",
							fileRepPrimaryHostAddress,
							fileRepPrimaryPort,
							fileRepMirrorHostAddress,
							fileRepMirrorPort,
							gp_segment_connect_timeout),
					 errSendAlert(true),
					 FileRep_errcontext()));

			status = STATUS_ERROR;
			continue;
		}

		/*
		 * check and process any signals received The routine returns TRUE if
		 * the received signal requests process shutdown.
		 */
		if (FileRepSubProcess_ProcessSignals())
		{
			continue;
		}

		if (retval < 0)
		{
			status = STATUS_ERROR;
			continue;
		}

		if (retval == 0)
		{
			continue;
		}

		Assert(retval > 0);

		status = FileRepConnServer_CreateConnection();

		if (status != STATUS_OK)
		{
			continue;
		}

		status = FileRepConnServer_ReceiveStartupPacket();
		if (status != STATUS_OK)
		{
			continue;
		}

		fileRepShmemArray[0]->state = FileRepStateInitialization;

		status = FileRepAckPrimary_RunReceiver();

	} //while (1)

		FileRepConnServer_CloseConnection();

	return;
}


/*
 *
 */
static int
FileRepAckPrimary_RunReceiver(void)
{
	uint32_t	msgLength = 0;
	FileRepConsumerProcIndex_e msgType;
	int			status = STATUS_OK;
	char	   *msgPositionInsert;
	FileRepShmemMessageDescr_s *fileRepShmemMessageDescr;
	uint32		spareField;

	FileRep_InsertConfigLogEntry("run receiver");

	while (1)
	{

		FileRepSubProcess_ProcessSignals();
		if (FileRepSubProcess_GetState() != FileRepStateReady &&
			FileRepSubProcess_GetState() != FileRepStateInitialization)
		{
			break;
		}

		if (!FileRepConnServer_AwaitMessageBegin())
		{
			/*
			 * call was interrupted ... go back to beginning to process
			 * signals
			 */
			continue;
		}

		status = FileRepConnServer_ReceiveMessageType(&msgType);

		if (status != STATUS_OK)
		{
			break;
		}

		/* DATA MESSAGE TYPE */
		status = FileRepConnServer_ReceiveMessageLength(&msgLength);

		if (status != STATUS_OK)
		{
			break;
		}

		msgPositionInsert = FileRep_ReserveShmem(fileRepAckShmemArray[msgType],
												 msgLength,
												  /* not used */ &spareField,
												 FileRepOperationNotSpecified,
												 FileRepAckShmemLock);

		if (msgPositionInsert == NULL)
		{

			status = STATUS_ERROR;
			ereport(WARNING,
					(errmsg("mirror failure, "
							"could not queue received ack message to be processed, "
							"failover requested"),
					 errhint("run gprecoverseg to re-establish mirror connectivity"),
					 FileRep_errdetail_Shmem(),
					 FileRep_errdetail_ShmemAck(),
					 FileRep_errcontext()));
			break;
		}

		status = FileRepConnServer_ReceiveMessageData(
													  msgPositionInsert + sizeof(FileRepShmemMessageDescr_s),
													  msgLength);

		if (status != STATUS_OK)
		{
			break;
		}

		SIMPLE_FAULT_INJECTOR(FileRepReceiver);

		fileRepShmemMessageDescr =
			(FileRepShmemMessageDescr_s *) msgPositionInsert;

		/* it is not in use */
		fileRepShmemMessageDescr->messageSync = FALSE;

		fileRepShmemMessageDescr->messageState = FileRepShmemMessageStateReady;

		LWLockAcquire(FileRepAckShmemLock, LW_EXCLUSIVE);

		FileRep_IpcSignal(fileRepIpcArray[fileRepAckShmemArray[msgType]->ipcArrayIndex]->semC,
						  &fileRepIpcArray[fileRepAckShmemArray[msgType]->ipcArrayIndex]->refCountSemC);

		LWLockRelease(FileRepAckShmemLock);

		FileRep_InsertLogEntry(
							   "P_RunReceiver",
							   FileRep_GetFlatFileIdentifier("", ""),
							   FileRepRelationTypeNotSpecified,
							   FileRepOperationNotSpecified,
							   FILEREP_UNDEFINED,
							   FILEREP_UNDEFINED,
							   FileRepAckStateNotInitialized,
							   spareField,
							   FILEREP_UNDEFINED);

	} //while (1)

		FileRepConnServer_CloseConnection();

	return status;
}

/****************************************************************
 * FILEREP SUB-PROCESS (FileRep Ack Primary CONSUMER Process)
 ****************************************************************/

Size
FileRepAckPrimary_ShmemSize(void)
{
	Size		size;

	size = hash_estimate_size(
							  (Size) FILEREP_MAX_OPEN_FILES,
							  sizeof(FileRepAckHashEntry_s));

	size = add_size(size, sizeof(FileRepAckHashShmem_s));

	return size;
}

/*
 * Initialize hash table of open files in private memory of FileRep process.
 * FileName of the file is the key of the hash table.
 * FileName is relative file path from $PGDATA directory.
 */
void
FileRepAckPrimary_ShmemInit(void)
{
	HASHCTL		hash_ctl;
	bool		foundPtr;

	fileRepAckHashShmem = (FileRepAckHashShmem_s *) ShmemInitStruct("filerep ack base hash",
																	sizeof(FileRepAckHashShmem_s),
																	&foundPtr);
	if (fileRepAckHashShmem == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 (errmsg("not enough shared memory for mirroring"))));
	}

	if (!foundPtr)
	{
		MemSet(fileRepAckHashShmem, 0, sizeof(FileRepAckHashShmem_s));
	}

	fileRepAckHashShmem->ipcArrayIndex = IndexIpcArrayAckHashShmem;

	MemSet(&hash_ctl, 0, sizeof(hash_ctl));
	hash_ctl.keysize = MAXPGPATH + 1;
	hash_ctl.entrysize = sizeof(FileRepAckHashEntry_s);
	hash_ctl.hash = string_hash;

	fileRepAckHashShmem->hash = ShmemInitHash("filerep ack hash",
											  FILEREP_MAX_OPEN_FILES,
											  FILEREP_MAX_OPEN_FILES,
											  &hash_ctl,
											  HASH_ELEM | HASH_FUNCTION);

	if (fileRepAckHashShmem->hash == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 (errmsg("not enough shared memory for mirroring"))));
	}

	return;
}

/*
 *
 */
void
FileRepAckPrimary_ShmemReInit(void)
{
	HASH_SEQ_STATUS hash_status;
	FileRepAckHashEntry_s *entry;

	if (fileRepAckHashShmem == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 (errmsg("not enough shared memory for mirroring"))));
	}

	fileRepAckHashShmem->ipcArrayIndex = IndexIpcArrayAckHashShmem;

	if (fileRepAckHashShmem->hash == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 (errmsg("not enough shared memory for mirroring"))));
	}

	LWLockAcquire(FileRepAckHashShmemLock, LW_EXCLUSIVE);

	hash_seq_init(&hash_status, fileRepAckHashShmem->hash);

	while ((entry = (FileRepAckHashEntry_s *) hash_seq_search(&hash_status)) != NULL)
	{

		FileRepAckPrimary_RemoveHashEntry(entry->fileName);
	}

	LWLockRelease(FileRepAckHashShmemLock);

	return;
}

/*
 * lookup ack entry
 */
static FileRepAckHashEntry_s *
FileRepAckPrimary_LookupHashEntry(FileName fileName)
{
	FileRepAckHashEntry_s *entry;
	char		key[MAXPGPATH + 1];

	snprintf(key, sizeof(key), "%s", fileName);

	Assert(fileRepAckHashShmem->hash != NULL);

	entry = (FileRepAckHashEntry_s *) hash_search(
												  fileRepAckHashShmem->hash,
												  (void *) &key,
												  HASH_FIND,
												  NULL);

	if (entry == NULL)
	{

		ereport(WARNING,
				(errmsg("mirror failure, "
						"could not lookup ack entry '%s' into ack table, no entry "
						"failover requested",
						(fileName == NULL) ? "<null>" : fileName),
				 errhint("run gprecoverseg to re-establish mirror connectivity"),
				 FileRep_errcontext()));
	}

	return entry;
}

/*
 * insert ack entry in hash table by caller of the mirror operation
 */
static FileRepAckHashEntry_s *
FileRepAckPrimary_InsertHashEntry(
								  FileName fileName,
								  bool *exists)
{

	bool		foundPtr;
	FileRepAckHashEntry_s *entry;
	char		key[MAXPGPATH + 1];

	snprintf(key, sizeof(key), "%s", fileName);

	Assert(fileRepAckHashShmem->hash != NULL);

	entry = (FileRepAckHashEntry_s *) hash_search(
												  fileRepAckHashShmem->hash,
												  (void *) &key,
												  HASH_ENTER_NULL,
												  &foundPtr);

	if (entry == NULL)
	{
		*exists = FALSE;
		return entry;
	}

	if (foundPtr)
	{
		*exists = TRUE;
	}
	else
	{
		*exists = FALSE;
	}

	return entry;
}

/*
 *
 *
 * fileName is relative path from $PGDATA directory
 */
static bool
FileRepAckPrimary_RemoveHashEntry(FileName fileName)
{
	/* NOTE check behavior when two users have concurrently open files */

	FileRepAckHashEntry_s *entry;
	bool		isRemoved = FALSE;
	char		key[MAXPGPATH + 1];

	snprintf(key, sizeof(key), "%s", fileName);

	Assert(fileRepAckHashShmem->hash != NULL);

	entry = (FileRepAckHashEntry_s *) hash_search(
												  fileRepAckHashShmem->hash,
												  (void *) &key,
												  HASH_REMOVE,
												  NULL);

	if (entry)
	{

		isRemoved = TRUE;
		FileRep_IpcSignal(fileRepIpcArray[fileRepAckHashShmem->ipcArrayIndex]->semC,
						  &fileRepIpcArray[fileRepAckHashShmem->ipcArrayIndex]->refCountSemC);

	}

	return isRemoved;
}

/*
 * It is called by backend process to insert new ack entry into hash table.
 */
int
FileRepAckPrimary_NewHashEntry(
							   FileRepIdentifier_u fileRepIdentifier,
							   FileRepOperation_e fileRepOperation,
							   FileRepRelationType_e fileRepRelationType)
{

	FileRepAckHashEntry_s *entry = NULL;
	bool		exists = FALSE;
	FileName	fileName = NULL;
	int			status = STATUS_OK;
	int			retry = 0;
	bool		wait = FALSE;

	fileName = FileRep_GetFileName(fileRepIdentifier, fileRepRelationType);

	while (FileRep_IsRetry(retry))
	{
		LWLockAcquire(FileRepAckHashShmemLock, LW_EXCLUSIVE);


		if (!FileRep_IsIpcSleep(fileRepOperation))
		{
			if (wait == TRUE)
			{
				wait = FALSE;
			}
		}

		entry = FileRepAckPrimary_InsertHashEntry(fileName, &exists);

		if (entry == NULL)
		{
			LWLockRelease(FileRepAckHashShmemLock);
			status = STATUS_ERROR;

			ereport(WARNING,
					(errmsg("mirror failure, "
							"could not insert ack entry into ack table, no memory "
							"failover requested"),
					 errhint("run gprecoverseg to re-establish mirror connectivity"),
					 FileRep_errdetail(fileRepIdentifier,
									   fileRepRelationType,
									   fileRepOperation,
									   FILEREP_UNDEFINED),
					 FileRep_errcontext()));

			goto exit;
		}

		if (exists)
		{
			if (!FileRep_IsIpcSleep(fileRepOperation))
			{
				fileRepIpcArray[fileRepAckHashShmem->ipcArrayIndex]->refCountSemC++;
				wait = TRUE;
			}

			LWLockRelease(FileRepAckHashShmemLock);
			if (FileRepSubProcess_IsStateTransitionRequested())
			{
				status = STATUS_ERROR;
				break;
			}

			if (FileRep_IsIpcSleep(fileRepOperation))
			{
				FileRep_Sleep1ms(retry);
				FileRep_IncrementRetry(retry);
			}
			else
			{
				FileRep_IpcWait(fileRepIpcArray[fileRepAckHashShmem->ipcArrayIndex]->semC, &fileRepIpcArray[fileRepAckHashShmem->ipcArrayIndex]->refCountSemC, FileRepAckHashShmemLock);
			}

			continue;
		}

		entry->fileRepOperation = fileRepOperation;
		entry->fileRepRelationType = fileRepRelationType;
		entry->fileRepAckState = FileRepAckStateWaiting;
		entry->xLogEof = xLogEof;
		entry->mirrorStatus = FileRepStatusSuccess;

		LWLockRelease(FileRepAckHashShmemLock);
		break;
	}

	if (exists)
	{
		status = STATUS_ERROR;

		ereport(WARNING,
				(errmsg("mirror failure, "
						"could not insert ack entry into ack table, entry exists "
						"failover requested"),
				 errhint("run gprecoverseg to re-establish mirror connectivity"),
				 FileRep_errdetail(fileRepIdentifier,
								   fileRepRelationType,
								   fileRepOperation,
								   FILEREP_UNDEFINED),
				 FileRep_errcontext()));
	}

exit:

	if (fileName)
	{
		pfree(fileName);
		fileName = NULL;
	}

	return status;
}

/*
 * Consumer process on Primary update the state in the hash entry
 */
static int
FileRepAckPrimary_UpdateHashEntry(
								  FileRepIdentifier_u fileRepIdentifier,
								  FileRepRelationType_e fileRepRelationType,
								  FileRepAckState_e fileRepAckState)
{

	FileRepAckHashEntry_s *entry;
	bool		exists;
	int			status = STATUS_OK;
	FileName	fileName = NULL;

	fileName = FileRep_GetFileName(fileRepIdentifier, fileRepRelationType);

	LWLockAcquire(FileRepAckHashShmemLock, LW_EXCLUSIVE);

	entry = FileRepAckPrimary_InsertHashEntry(fileName, &exists);

	/* entry should be found since ack has not been processed yet */
	Assert(entry != NULL);

	if (!exists)
	{
		LWLockRelease(FileRepAckHashShmemLock);

		if (fileRepRelationType == FileRepRelationTypeUnknown)
		{
			goto exit;
		}

		status = STATUS_ERROR;
		ereport(WARNING,
				(errmsg("mirror failure, "
						"could not update ack entry with ack status, no entry found "
						"identifier '%s' relation type '%s' "
						"failover requested",
						(fileName == NULL) ? "<null>" : fileName,
						FileRepRelationTypeToString[fileRepRelationType]),
				 errhint("run gprecoverseg to re-establish mirror connectivity"),
				 FileRep_errcontext()));

		goto exit;
	}

	entry->fileRepAckState = fileRepAckState;

	entry->xLogEof = xLogEof;
	entry->mirrorStatus = mirrorStatus;

	FileRep_IpcSignal(fileRepIpcArray[fileRepAckHashShmem->ipcArrayIndex]->semP,
					  &fileRepIpcArray[fileRepAckHashShmem->ipcArrayIndex]->refCountSemP);

	LWLockRelease(FileRepAckHashShmemLock);

	if (Debug_filerep_print)
		ereport(LOG,
				(errmsg("update hash entry identifier '%s' ack state '%s' ",
						(entry->fileName == NULL) ? "<null>" : entry->fileName,
						FileRepAckStateToString[entry->fileRepAckState])));

exit:
	if (fileName)
	{
		pfree(fileName);
		fileName = NULL;
	}

	return status;
}

/*
 * Backend checks if acknowledgement that its operation is completed
 * is received from mirror.
 * If acknowledgement is received (state == FileRepAckStateCompleted) then
 *			a) entry is removed from hash
 *			b) TRUE is returned
 */
bool
FileRepAckPrimary_IsOperationCompleted(
									   FileRepIdentifier_u fileRepIdentifier,
									   FileRepRelationType_e fileRepRelationType)
{

	FileRepAckHashEntry_s *entry = NULL;
	bool		isCompleted = FALSE;
	bool		isRemoved;
	FileName	fileName = NULL;
	int			retry = 0;
	bool		retval = FALSE;
	bool		wait = FALSE;

	fileName = FileRep_GetFileName(fileRepIdentifier, fileRepRelationType);

	while ((isCompleted == FALSE) && FileRep_IsRetry(retry))
	{

		LWLockAcquire(FileRepAckHashShmemLock, LW_EXCLUSIVE);

		entry = FileRepAckPrimary_LookupHashEntry(fileName);

		if (entry == NULL)
		{
			LWLockRelease(FileRepAckHashShmemLock);
			break;
		}

		if (!FileRep_IsIpcSleep(entry->fileRepOperation))
		{
			if (wait == TRUE)
			{
				wait = FALSE;
			}
		}

		switch (entry->fileRepAckState)
		{
			case FileRepAckStateWaiting:
				/* No Operation */
				break;
			case FileRepAckStateCompleted:

				retval = TRUE;
				xLogEof = entry->xLogEof;
				mirrorStatus = entry->mirrorStatus;
				/* no BREAK */
			case FileRepAckStateMirrorInFault:

				isCompleted = TRUE;

				isRemoved = FileRepAckPrimary_RemoveHashEntry(fileName);

				Assert(isRemoved == TRUE);

				break;
			default:
				break;
		}

		if (isCompleted == false)
		{
			if (!FileRep_IsIpcSleep(entry->fileRepOperation))
			{
				fileRepIpcArray[fileRepAckHashShmem->ipcArrayIndex]->refCountSemP++;
				wait = TRUE;
			}
		}

		LWLockRelease(FileRepAckHashShmemLock);

		if (isCompleted == false)
		{
			if (FileRepSubProcess_IsStateTransitionRequested())
			{
				break;
			}

			if (FileRep_IsIpcSleep(entry->fileRepOperation))
			{
				FileRep_Sleep1ms(retry);

				if (retry == (3 * file_rep_retry / 4))
					ereport(WARNING,
							(errmsg("threshold '75' percent of 'gp_segment_connect_timeout=%d' is reached, "
									"mirror may not be able to keep up with primary, "
									"primary may transition to change tracking",
									gp_segment_connect_timeout),
							 errhint("increase guc 'gp_segment_connect_timeout' by 'gpconfig' and 'gpstop -u' "),
							 errSendAlert(true)));

				FileRep_IncrementRetry(retry);
			}
			else
			{
				FileRep_IpcWait(fileRepIpcArray[fileRepAckHashShmem->ipcArrayIndex]->semP, &fileRepIpcArray[fileRepAckHashShmem->ipcArrayIndex]->refCountSemP, FileRepAckHashShmemLock);
			}

			/*
			 * if the message was from the main filerep process then it is a
			 * graceful shutdown message to the mirror.  We don't want to
			 * stall shutdown if the mirror is unavailable so we wait a
			 * smaller amount of time
			 */
			if (entry->fileRepOperation == FileRepOperationShutdown &&
				retry == 50)
			{
				FileRepAckPrimary_RemoveHashEntry(fileName);
				break;
			}
		}
	}

	if (retval == FALSE)
	{
		mirrorStatus = FileRepStatusMirrorLossOccurred;

		if (!primaryMirrorIsIOSuspended())
		{
			ereport(WARNING,
					(errmsg("mirror failure, "
							"could not complete mirrored request identifier '%s' ack state '%s', "
							"failover requested",
							(fileName == NULL) ? "<null>" : fileName,
							(entry == NULL) ? "<entry not found>" : FileRepAckStateToString[entry->fileRepAckState]),
					 errhint("run gprecoverseg to re-establish mirror connectivity"),
					 FileRep_errdetail_ShmemAck(),
					 FileRep_errcontext()));
		}
	}

	if (fileName)
	{
		pfree(fileName);
		fileName = NULL;
	}

	return retval;
}


/*
 *
 * FileRepAckPrimary_StartConsumer
*/
void
FileRepAckPrimary_StartConsumer(void)
{
	int			status = STATUS_OK;

	FileRep_InsertConfigLogEntry("run consumer");

	while (1)
	{

		if (status != STATUS_OK)
		{
			FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
			FileRepSubProcess_SetState(FileRepStateFault);
		}

		while (FileRepSubProcess_GetState() == FileRepStateFault ||

			   (fileRepShmemArray[0]->state == FileRepStateNotInitialized &&
				FileRepSubProcess_GetState() != FileRepStateShutdown))
		{

			FileRepSubProcess_ProcessSignals();
			pg_usleep(50000L);	/* 50 ms */
		}

		if (FileRepSubProcess_GetState() == FileRepStateShutdown)
		{

			break;
		}

		status = FileRepAckPrimary_RunConsumer();

	} //while (1)


		if (FileRepSubProcess_GetState() == FileRepStateShutdown)
		{
			/* perform graceful shutdown */
		}

	LWLockAcquire(FileRepAckHashShmemLock, LW_EXCLUSIVE);

	FileRep_IpcSignal(fileRepIpcArray[fileRepAckHashShmem->ipcArrayIndex]->semP,
					  &fileRepIpcArray[fileRepAckHashShmem->ipcArrayIndex]->refCountSemP);

	LWLockRelease(FileRepAckHashShmemLock);

	/* NOTE free memory (if any) */
	return;
}


/*
 * FileRepAckPrimary_RunConsumer()
 */
static int
FileRepAckPrimary_RunConsumer(void)
{
	FileRepShmemMessageDescr_s *fileRepShmemMessageDescr = NULL;
	FileRepMessageHeader_s *fileRepMessageHeader = NULL;
	pg_crc32   *fileRepMessageHeaderCrc;
	pg_crc32	messageHeaderCrcLocal = 0;
	int			status = STATUS_OK;
	bool		movePositionConsume = FALSE;
	FileRepShmem_s *fileRepAckShmem = NULL;

	FileRep_InsertConfigLogEntry("run consumer");

	fileRepAckShmem = fileRepAckShmemArray[FILEREP_ACKSHMEM_MESSAGE_SLOT_PRIMARY_ACK];

	while (1)
	{

		LWLockAcquire(FileRepAckShmemLock, LW_EXCLUSIVE);

		if (movePositionConsume)
		{

			fileRepAckShmem->positionConsume =
				fileRepAckShmem->positionConsume +
				fileRepShmemMessageDescr->messageLength +
				sizeof(FileRepShmemMessageDescr_s);

			if (fileRepAckShmem->positionConsume == fileRepAckShmem->positionWraparound &&
				fileRepAckShmem->positionInsert != fileRepAckShmem->positionWraparound)
			{

				fileRepAckShmem->positionConsume = fileRepAckShmem->positionBegin;
				fileRepAckShmem->positionWraparound = fileRepAckShmem->positionEnd;
			}
			FileRep_IpcSignal(fileRepIpcArray[fileRepAckShmem->ipcArrayIndex]->semP,
							  &fileRepIpcArray[fileRepAckShmem->ipcArrayIndex]->refCountSemP);
		}

		fileRepShmemMessageDescr =
			(FileRepShmemMessageDescr_s *) fileRepAckShmem->positionConsume;

		while ((fileRepAckShmem->positionConsume == fileRepAckShmem->positionInsert) ||
			   ((fileRepAckShmem->positionConsume != fileRepAckShmem->positionInsert) &&
				(fileRepShmemMessageDescr->messageState != FileRepShmemMessageStateReady)))
		{

			fileRepIpcArray[fileRepAckShmem->ipcArrayIndex]->refCountSemC++;

			LWLockRelease(FileRepAckShmemLock);

			FileRepSubProcess_ProcessSignals();
			if (FileRepSubProcess_GetState() != FileRepStateReady &&
				FileRepSubProcess_GetState() != FileRepStateInitialization)
			{
				LWLockAcquire(FileRepAckShmemLock, LW_EXCLUSIVE);
				break;
			}

			FileRep_IpcWait(fileRepIpcArray[fileRepAckShmem->ipcArrayIndex]->semC, &fileRepIpcArray[fileRepAckShmem->ipcArrayIndex]->refCountSemC, FileRepAckShmemLock);

			LWLockAcquire(FileRepAckShmemLock, LW_EXCLUSIVE);

			if (fileRepAckShmem->positionConsume == fileRepAckShmem->positionWraparound &&
				fileRepAckShmem->positionInsert != fileRepAckShmem->positionWraparound)
			{

				fileRepAckShmem->positionConsume = fileRepAckShmem->positionBegin;
				fileRepAckShmem->positionWraparound = fileRepAckShmem->positionEnd;
			}

			/* Re-assign to find if messageState is changed */
			fileRepShmemMessageDescr =
				(FileRepShmemMessageDescr_s *) fileRepAckShmem->positionConsume;

		} //internal while
			fileRepAckShmem
			->consumeCount++;
		LWLockRelease(FileRepAckShmemLock);

		FileRepSubProcess_ProcessSignals();
		if (FileRepSubProcess_GetState() != FileRepStateReady &&
			FileRepSubProcess_GetState() != FileRepStateInitialization)
		{
			break;
		}

		SIMPLE_FAULT_INJECTOR(FileRepConsumer);

		/* Calculate and compare FileRepMessageHeader_s Crc */
		fileRepMessageHeader = (FileRepMessageHeader_s *) (fileRepAckShmem->positionConsume +
														   sizeof(FileRepShmemMessageDescr_s));

		FileRep_CalculateCrc((char *) fileRepMessageHeader,
							 sizeof(FileRepMessageHeader_s),
							 &messageHeaderCrcLocal);

		fileRepMessageHeaderCrc =
			(pg_crc32 *) (fileRepAckShmem->positionConsume +
						  sizeof(FileRepMessageHeader_s) +
						  sizeof(FileRepShmemMessageDescr_s));

		if (*fileRepMessageHeaderCrc != messageHeaderCrcLocal)
		{
			status = STATUS_ERROR;
			ereport(WARNING,
					(errmsg("mirror failure, "
							"could not match ack message header checksum between primary '%u' and mirror '%u', "
							"failover requested",
							*fileRepMessageHeaderCrc,
							messageHeaderCrcLocal),
					 errhint("run gprecoverseg to re-establish mirror connectivity"),
					 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
									   fileRepMessageHeader->fileRepRelationType,
									   fileRepMessageHeader->fileRepOperation,
									   fileRepMessageHeader->messageCount),
					 FileRep_errdetail_ShmemAck(),
					 FileRep_errcontext()));

			break;
		}

		/*
		 * Write operation is never acknowledged. That means message should
		 * never have body. CRC of body should be always 0.
		 */
		Assert(fileRepMessageHeader->fileRepOperation != FileRepOperationWrite);
		Assert(fileRepMessageHeader->fileRepMessageBodyCrc == 0);

		switch (fileRepMessageHeader->fileRepOperation)
		{
			case FileRepOperationReconcileXLogEof:
				xLogEof = fileRepMessageHeader->fileRepOperationDescription.reconcile.xLogEof;

				if (Debug_filerep_print)
					ereport(LOG,
							(errmsg("ack reconcile xlogid '%d' xrecoff '%d' ",
									xLogEof.xlogid,
									xLogEof.xrecoff)));

				break;

			case FileRepOperationValidation:
				mirrorStatus = fileRepMessageHeader->fileRepOperationDescription.validation.mirrorStatus;

				if (Debug_filerep_print)
					ereport(LOG,
							(errmsg("ack validation status '%s' ",
									FileRepStatusToString[mirrorStatus])));

				break;

			case FileRepOperationCreate:
				mirrorStatus = fileRepMessageHeader->fileRepOperationDescription.create.mirrorStatus;

				if (Debug_filerep_print)
					ereport(LOG,
							(errmsg("ack create status '%s' ",
									FileRepStatusToString[mirrorStatus])));

				break;

			case FileRepOperationStartSlruChecksum:
				mirrorStatus =
					fileRepMessageHeader->fileRepOperationDescription.startChecksum.mirrorStatus;

				if (Debug_filerep_print)
				{
					ereport(LOG,
							(errmsg("ack start SLRU checksum: status = '%s', directory = '%s' ",
									FileRepStatusToString[mirrorStatus],
									fileRepMessageHeader->fileRepIdentifier.fileRepFlatFileIdentifier.directorySimpleName)));
				}

				break;

			case FileRepOperationVerifySlruDirectoryChecksum:
				mirrorStatus =
					fileRepMessageHeader->fileRepOperationDescription.verifyDirectoryChecksum.mirrorStatus;

				if (Debug_filerep_print)
				{
					ereport(LOG,
							(errmsg("ack verify SLRU directory checksum: status = '%s', directory = '%s' ",
									FileRepStatusToString[mirrorStatus],
									fileRepMessageHeader->fileRepIdentifier.fileRepFlatFileIdentifier.directorySimpleName)));
				}

				break;

			default:
				break;
		}

		if (fileRepMessageHeader->fileRepAckState != FileRepAckStateCompleted)
		{

			status = STATUS_ERROR;

			ereport(WARNING,
					(errmsg("mirror failure, "
							"could not complete operation on mirror ack state '%s', "
							"failover requested",
							FileRepAckStateToString[fileRepMessageHeader->fileRepAckState]),
					 errhint("run gprecoverseg to re-establish mirror connectivity"),
					 errSendAlert(true),
					 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
									   fileRepMessageHeader->fileRepRelationType,
									   fileRepMessageHeader->fileRepOperation,
									   fileRepMessageHeader->messageCount),
					 FileRep_errdetail_Shmem(),
					 FileRep_errdetail_ShmemAck(),
					 FileRep_errcontext()));

			/*
			 * FAULT has to be set before entry is updated in ack hash table
			 * in order to suspend backend process.
			 */
			FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
			FileRepSubProcess_ProcessSignals();
		}

		if (FileRepAckPrimary_UpdateHashEntry(
											  fileRepMessageHeader->fileRepIdentifier,
											  fileRepMessageHeader->fileRepRelationType,
											  fileRepMessageHeader->fileRepAckState) != STATUS_OK)
		{

			status = STATUS_ERROR;
			ereport(WARNING,
					(errmsg("mirror failure, "
							"could not update ack state '%s' in ack hash table, "
							"failover requested",
							FileRepAckStateToString[fileRepMessageHeader->fileRepAckState]),
					 errhint("run gprecoverseg to re-establish mirror connectivity"),
					 errSendAlert(true),
					 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
									   fileRepMessageHeader->fileRepRelationType,
									   fileRepMessageHeader->fileRepOperation,
									   fileRepMessageHeader->messageCount),
					 FileRep_errdetail_Shmem(),
					 FileRep_errdetail_ShmemAck(),
					 FileRep_errcontext()));
		}

		FileRep_InsertLogEntry(
							   "P_RunConsumer",
							   fileRepMessageHeader->fileRepIdentifier,
							   fileRepMessageHeader->fileRepRelationType,
							   fileRepMessageHeader->fileRepOperation,
							   messageHeaderCrcLocal,
							   fileRepMessageHeader->fileRepMessageBodyCrc,
							   fileRepMessageHeader->fileRepAckState,
							   FILEREP_UNDEFINED,
							   fileRepMessageHeader->messageCount);

		if (status != STATUS_OK)
		{
			break;
		}

		movePositionConsume = TRUE;
	} //while (1)

		return status;
}

XLogRecPtr
FileRepAckPrimary_GetMirrorXLogEof(void)
{
	return xLogEof;
}

int
FileRepAckPrimary_GetMirrorErrno(void)
{
	return mirrorStatus;
}
