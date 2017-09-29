/*-------------------------------------------------------------------------
 *
 * cdbfilerepmirror.c
 *
 * Portions Copyright (c) 2009-2010 Greenplum Inc. All rights reserved.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbfilerepmirror.c
 *
 *-------------------------------------------------------------------------
 */

/*
 * FileRep process (receiver and consumer threads) launched by main thread.
 *
 * Responsibilities of this module
 *		*)
 *
 */
#include "postgres.h"
#include <signal.h>

#include "port.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <glob.h>
#include <signal.h>
#include <sys/time.h>

#include "cdb/cdbvars.h"

#include "access/xlogdefs.h"
#include "access/slru.h"
#include "cdb/cdbfilerepservice.h"
#include "cdb/cdbfilerepmirror.h"
#include "cdb/cdbfilerepmirrorack.h"
#include "cdb/cdbfilerep.h"
#include "cdb/cdbfilerepconnserver.h"
#include "cdb/cdbmirroredflatfile.h"
#include "libpq/pqsignal.h"
#include "storage/lwlock.h"
#include "utils/faultinjector.h"
#include "utils/hsearch.h"
#include "utils/pg_crc.h"

/*
 * It specifies fields maintained by each entry in hash table
 * of open files.
 */
typedef struct OpenFileEntry
{
	/* NOTE check it should be char fileName[MAXPGPATH] */
	char		fileName[MAXPGPATH + 1];
	/* It is key. It has to be first field in the structure. */

	File		fd;
	/* File Descriptor */

	FileRepRelationType_e fileRepRelationType;
	/* Type of the file. It is required for consistency check. */

	int			reference;		/* NOTE check if multiple open will have also
								 * multiple close */

	/*
	 * Tracks number of transactions that have concurrently opened file. File
	 * types that can be opened concurrently by multiple TX are a) Flat File
	 * b) Buffer Pool
	 *
	 * File types that are not allowed to be opened concurrently by multiple
	 * TX are a) Append Only b) Bulk
	 *
	 */

} OpenFileEntry;


static HTAB *openFilesTable = NULL;


static int	FileRepMirror_Init(void);
static void FileRepMirror_Cleanup(void);

static int	FileRepMirror_RunReceiver(void);
static int	FileRepMirror_RunConsumer(void);

static FileRepShmem_s *fileRepShmem = NULL;

/*
 * Find file descriptor in hash table of open files.
 */
static File FileRepMirror_GetFileDescriptor(FileName fileName);

/*
 * Lookup for entry in hash table of open files.
 */
static OpenFileEntry *FileRepMirror_LookupFileName(FileName fileName);

/*
 * Insert entry with info related to new open file in hash table of open files.
 */
static int FileRepMirror_InsertFileName(
							 FileName fileName,
							 File fd,
							 FileRepRelationType_e fileRepRelationType);

/*
 * Remove entry from hash table of opend files.
 */
static void FileRepMirror_RemoveFileName(FileName fileName);

static void FileRepMirror_ReInit(void);

static int	FileRepMirror_Validation(FileName fileName);

static void FileRepMirror_DropFilesFromDir(FileName fileName);

static int	SegmentFileFillZero(File fd, int64 writeOffset);

static int	FileRepMirror_DropBufferPoolSegmentFiles(FileName fileName, int segmentFileNum);

static int	FileRepMirror_Drop(FileName fileName);

/****************************************************************
 * FILEREP SUB-PROCESS (FileRep Mirror RECEIVER Process)
 ****************************************************************/

/*
 *
 * FileRepMirror_StartReceiver
 */
void
FileRepMirror_StartReceiver(void)
{
	int			status = STATUS_OK;
	struct timeval currentTime;
	pg_time_t	beginTime = 0;
	pg_time_t	endTime = 0;
	int			retval = 0;

	FileRep_InsertConfigLogEntry("start receiver");

	{
		char		tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];

		snprintf(tmpBuf, sizeof(tmpBuf), "primary address(port) '%s(%d)' mirror address(port) '%s(%d)' ",
				 fileRepPrimaryHostAddress,
				 fileRepPrimaryPort,
				 fileRepMirrorHostAddress,
				 fileRepMirrorPort);

		FileRep_InsertConfigLogEntry(tmpBuf);
	}

	Insist(fileRepRole == FileRepMirrorRole);

	status = FileRepConnServer_StartListener(
											 fileRepMirrorHostAddress,
											 fileRepMirrorPort);
	if (status == STATUS_OK)
	{
		FileRepSubProcess_SetState(FileRepStateReady);
	}

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
							"no connection was established from client from primary, "
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

		status = FileRepMirror_RunReceiver();

	} //while (1)

		FileRepConnServer_CloseConnection();

	return;
}

enum mirror_receiver_state
{
	RECVTYPE, RECVLEN, RECVBODY
};

/*
 *
 */
static int
FileRepMirror_RunReceiver(void)
{
	uint32_t	msgLength = 0;
	FileRepConsumerProcIndex_e msgType;
	int			status = STATUS_OK;
	char	   *msgPositionInsert = NULL;
	FileRepShmemMessageDescr_s *fileRepShmemMessageDescr;
	uint32		spareField;
	bool		waitShutdown = FALSE;
	bool		requestShutdown = FALSE;

	enum mirror_receiver_state current_state = RECVTYPE;

	FileRep_InsertConfigLogEntry("run receiver");

	for (;;)
	{
		FileRepSubProcess_ProcessSignals();
		if (FileRepSubProcess_GetState() != FileRepStateReady)
		{
			break;
		}

		if (!FileRepConnServer_AwaitMessageBegin())
			continue;

		if (waitShutdown == TRUE)
		{
			pg_usleep(50000L);
			/* 50 ms */
			continue;
		}

		status = STATUS_OK;

		switch (current_state)
		{
			case RECVTYPE:
				status = FileRepConnServer_ReceiveMessageType(&msgType);

				if (status == STATUS_OK)
					current_state = RECVLEN;

				if (msgType == FileRepMessageTypeShutdown)
				{
					msgType = FileRepMessageTypeXLog;
					requestShutdown = TRUE;

					FileRep_InsertLogEntry(
										   "M_RunReceiver",
										   FileRep_GetFlatFileIdentifier("", ""),
										   FileRepRelationTypeNotSpecified,
										   FileRepOperationShutdown,
										   FILEREP_UNDEFINED,
										   FILEREP_UNDEFINED,
										   FileRepAckStateNotInitialized,
										   spareField,
										   FILEREP_UNDEFINED);
				}

				break;
			case RECVLEN:
				/* DATA MESSAGE TYPE */
				status = FileRepConnServer_ReceiveMessageLength(&msgLength);

				if (status != STATUS_OK)
				{
					break;
				}

				msgPositionInsert = FileRep_ReserveShmem(
														 fileRepShmemArray[msgType],
														 msgLength,
														 &spareField,
														 FileRepOperationNotSpecified,
														 FileRepShmemLock);

				if (msgPositionInsert == NULL)
				{
					status = STATUS_ERROR;
					ereport(WARNING,
							(errmsg("mirror failure, "
									"could not queue received message to be processed, "
									"failover requested"),
							 errhint("run gprecoverseg to re-establish mirror connectivity"),
							 FileRep_errdetail_Shmem(),
							 FileRep_errcontext()));

					break;
				}
				if (status == STATUS_OK)
					current_state = RECVBODY;

				break;
			case RECVBODY:
				status = FileRepConnServer_ReceiveMessageData(
															  msgPositionInsert + sizeof(FileRepShmemMessageDescr_s),
															  msgLength);

				if (status != STATUS_OK)
				{
					break;
				}

				FileRep_InsertLogEntry(
									   "M_RunReceiver",
									   FileRep_GetFlatFileIdentifier("", ""),
									   FileRepRelationTypeNotSpecified,
									   FileRepOperationNotSpecified,
									   FILEREP_UNDEFINED,
									   FILEREP_UNDEFINED,
									   FileRepAckStateNotInitialized,
									   spareField,
									   FILEREP_UNDEFINED);

				SIMPLE_FAULT_INJECTOR(FileRepReceiver);

				fileRepShmemMessageDescr = (FileRepShmemMessageDescr_s *) msgPositionInsert;

				/* it is not in use */
				fileRepShmemMessageDescr->messageSync = FALSE;
				fileRepShmemMessageDescr->messageState = FileRepShmemMessageStateReady;

				LWLockAcquire(FileRepShmemLock, LW_EXCLUSIVE);
				FileRep_IpcSignal(fileRepIpcArray[fileRepShmemArray[msgType]->ipcArrayIndex]->semC,
								  &fileRepIpcArray[fileRepShmemArray[msgType]->ipcArrayIndex]->refCountSemC);
				LWLockRelease(FileRepShmemLock);

				if (status == STATUS_OK)
					current_state = RECVTYPE;

				if (requestShutdown == TRUE)
					waitShutdown = TRUE;

				break;

			default:
				status = STATUS_ERROR;
				break;
				break;
		}

		if (status != STATUS_OK)
		{
			break;
		}
	}							/* for (;;) */

	FileRepConnServer_CloseConnection();

	return status;
}

/****************************************************************
 * FILEREP SUB-PROCESS (FileRep Mirror CONSUMER Process)
 ****************************************************************/

/* NOTES
 *		thread pool
 *		have protection between open files and fd inserted in hash table
 *		have protection to shared memory consumption
 *		decrement reference and removing HASH entry should be atomic
 *      check if reference is required (multiple open result in multiple close)
 *      see example cdbdisp.c (call gp_pthread_create)
 *		see www.kernel.org (pthread_create)
 */


/*
 * Initialize hash table of open files in private memory of FileRep process.
 * FileName of the file is the key of the hash table.
 * FileName is relative file path from $PGDATA directory.
 */
static int
FileRepMirror_Init(void)
{
	HASHCTL		hash_ctl;
	int			status = STATUS_OK;

	MemSet(&hash_ctl, 0, sizeof(hash_ctl));
	hash_ctl.keysize = MAXPGPATH;
	hash_ctl.entrysize = sizeof(OpenFileEntry);
	hash_ctl.hash = string_hash;

	openFilesTable = hash_create("open files hash",
								 FILEREP_MAX_OPEN_FILES,
								 &hash_ctl,
								 HASH_ELEM | HASH_FUNCTION);

	if (openFilesTable == NULL)
	{
		status = STATUS_ERROR;
	}

	return status;
}

/*
 *	a) Close files that are in the hash table.
 *	b) Remove hash table (free memory).
 */
static void
FileRepMirror_Cleanup(void)
{
	HASH_SEQ_STATUS hash_status;
	OpenFileEntry *entry;

	if (openFilesTable == NULL)
	{
		return;
	}

	hash_seq_init(&hash_status, openFilesTable);

	while ((entry = (OpenFileEntry *) hash_seq_search(&hash_status)) != NULL)
	{
		Assert(entry->fd > 0);
		FileClose(entry->fd);
	}

	hash_destroy(openFilesTable);

	return;
}

/*
 *	Remove hash entry.
 */
static void
FileRepMirror_ReInit(void)
{
	HASH_SEQ_STATUS hash_status;
	OpenFileEntry *entry;

	if (openFilesTable == NULL)
	{
		return;
	}

	hash_seq_init(&hash_status, openFilesTable);

	while ((entry = (OpenFileEntry *) hash_seq_search(&hash_status)) != NULL)
	{
		FileRepMirror_RemoveFileName(entry->fileName);
	}

	return;
}

/*
 *
 * FileRepMirror_StartConsumer
 */
void
FileRepMirror_StartConsumer(void)
{
	int			status = STATUS_OK;

	FileRep_InsertConfigLogEntry("start consumer");

	FileRepMirror_ReInit();

	switch (fileRepProcessType)
	{
		case FileRepProcessTypeMirrorConsumer:
			fileRepProcIndex = FileRepMessageTypeXLog;
			break;
		case FileRepProcessTypeMirrorConsumerWriter:
			fileRepProcIndex = FileRepMessageTypeWriter;
			break;
		case FileRepProcessTypeMirrorConsumerAppendOnly1:
			fileRepProcIndex = FileRepMessageTypeAO01;
			break;
		default:
			Assert(0);
			status = STATUS_ERROR;
			break;
	}

	fileRepShmem = fileRepShmemArray[fileRepProcIndex];

	status = FileRepMirror_Init();

	while (1)
	{

		if (status != STATUS_OK)
		{
			FileRep_SetSegmentState(SegmentStateFault, FaultTypeIO);
			FileRepSubProcess_SetState(FileRepStateFault);
		}

		while (FileRepSubProcess_GetState() == FileRepStateInitialization ||
			   FileRepSubProcess_GetState() == FileRepStateFault)
		{

			FileRepSubProcess_ProcessSignals();
			pg_usleep(50000L);	/* 50 ms */
		}

		if (FileRepSubProcess_GetState() == FileRepStateShutdown)
		{

			break;
		}

		Insist(FileRepSubProcess_GetState() == FileRepStateReady);

		status = FileRepMirror_RunConsumer();

	} //while (1)
		if (FileRepSubProcess_GetState() == FileRepStateShutdown)
		{
			/* perform graceful shutdown */
		}

	/* NOTE free memory (if any) */
	FileRepMirror_Cleanup();

	return;
}


/*
 *		NOTE: Draw state machine
 *
 *		a) File create
 *				After create the file is closed.
 *		b) File delete/drop
 *				Before delete the file must be closed
 *				otherwise error will be returned.
 *		c) File open
 *				Before open the file must be created. If private relation then
 *				file mustn't be already opened.
 *		d) File write
 *				Before write the file must be opened.
 *		e) File close
 *				Before close the file must be opened.
 *		f) Directory create
 *				Before create the parent directory must have the right permissions.
 *		g) Directory delete/drop
 *				Before delete the directory must be empty.
 */
static int
FileRepMirror_RunConsumer(void)
{
	FileRepShmemMessageDescr_s *fileRepShmemMessageDescr = NULL;
	FileRepMessageHeader_s *fileRepMessageHeader;
	pg_crc32   *fileRepMessageHeaderCrc;
	char	   *fileRepMessageBody = NULL;
	pg_crc32	messageHeaderCrcLocal;
	pg_crc32	messageBodyLocal;
	File		fd = 0;
	FileName	fileName = NULL;
	FileName	newFileName;
	int			fileMode = 0666;

	/* change to 0600 */
	int			status = STATUS_OK;
	int64		seekResult;
	bool		found = FALSE;
	OpenFileEntry *entry = NULL;
	bool		movePositionConsume = FALSE;
	XLogRecPtr	mirrorEofRecPtr = {0, 0};
	uint32		spare = 0;
	struct timeval currentTime;
	pg_time_t	beginTime = 0;
	pg_time_t	endTime = 0;
	void	   *responseData = NULL;

	FileRep_InsertConfigLogEntry("run consumer");

	errno = 0;

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
			if (FileRepSubProcess_GetState() != FileRepStateReady)
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
		if (FileRepSubProcess_GetState() != FileRepStateReady)
		{
			break;
		}

		SIMPLE_FAULT_INJECTOR(FileRepConsumer);

		/* Calculate and compare FileRepMessageHeader_s Crc */
		fileRepMessageHeader = (FileRepMessageHeader_s *) (fileRepShmem->positionConsume +
														   sizeof(FileRepShmemMessageDescr_s));

		FileRep_CalculateCrc((char *) fileRepMessageHeader,
							 sizeof(FileRepMessageHeader_s),
							 &messageHeaderCrcLocal);

		fileRepMessageHeaderCrc =
			(pg_crc32 *) (fileRepShmem->positionConsume +
						  sizeof(FileRepMessageHeader_s) +
						  sizeof(FileRepShmemMessageDescr_s));

		if (*fileRepMessageHeaderCrc != messageHeaderCrcLocal)
		{

			status = STATUS_ERROR;
			ereport(WARNING,
					(errmsg("mirror failure, "
							"could not match message header checksum between primary '%u' and mirror '%u', "
							"failover requested",
							*fileRepMessageHeaderCrc,
							messageHeaderCrcLocal),
					 errhint("run gprecoverseg to re-establish mirror connectivity"),
					 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
									   fileRepMessageHeader->fileRepRelationType,
									   fileRepMessageHeader->fileRepOperation,
									   fileRepMessageHeader->messageCount),
					 FileRep_errdetail_Shmem(),
					 FileRep_errcontext()));

			FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);

			break;
		}

		/* if message body present then calculate and compare Data Crc */

		if (fileRepMessageHeader->fileRepOperation == FileRepOperationWrite)
		{

			if (fileRepMessageHeader->messageBodyLength > 0)
			{

				fileRepMessageBody =
					(char *) (fileRepShmem->positionConsume +
							  sizeof(FileRepMessageHeader_s) +
							  sizeof(FileRepShmemMessageDescr_s) +
							  sizeof(pg_crc32));

				FileRep_CalculateCrc((char *) fileRepMessageBody,
									 fileRepMessageHeader->messageBodyLength,
									 &messageBodyLocal);

				if (messageBodyLocal != fileRepMessageHeader->fileRepMessageBodyCrc)
				{
					ereport(WARNING,
							(errmsg("mirror failure, "
									"could not match message body checksum between primary:'%u' and mirror:'%u' data length:'%u', "
									"failover requested",
									fileRepMessageHeader->fileRepMessageBodyCrc,
									messageBodyLocal,
									fileRepMessageHeader->messageBodyLength),
							 errhint("run gprecoverseg to re-establish mirror connectivity"),

							 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
											   fileRepMessageHeader->fileRepRelationType,
											   fileRepMessageHeader->fileRepOperation,
											   fileRepMessageHeader->messageCount),
							 FileRep_errdetail_Shmem(),
							 FileRep_errcontext()));

					status = STATUS_ERROR;

					fileRepMessageHeader->messageBodyLength = 0;

					FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);

					break;
				}
				fileRepMessageHeader->messageBodyLength = 0;
			}
		}

		FileRep_InsertLogEntry(
							   "M_RunConsumer",
							   fileRepMessageHeader->fileRepIdentifier,
							   fileRepMessageHeader->fileRepRelationType,
							   fileRepMessageHeader->fileRepOperation,
							   messageHeaderCrcLocal,
							   fileRepMessageHeader->fileRepMessageBodyCrc,
							   FileRepAckStateNotInitialized,
							   spare,
							   fileRepMessageHeader->messageCount);

		spare = fileRepMessageHeader->messageCount;

		/* fileName is relative path to $PGDATA directory */
		fileName = FileRep_GetFileName(
									   fileRepMessageHeader->fileRepIdentifier,
									   fileRepMessageHeader->fileRepRelationType);

		switch (fileRepMessageHeader->fileRepOperation)
		{
			case FileRepOperationOpen:
			case FileRepOperationCreateAndOpen:
				found = FALSE;

				/*
				 * MPP-16542 : mask off O_DIRECT for filerep mirror shmem
				 * alignment issue
				 */
				if (fileRepMessageHeader->fileRepOperationDescription.open.fileFlags & PG_O_DIRECT)
					fileRepMessageHeader->fileRepOperationDescription.open.fileFlags &= ~PG_O_DIRECT;

				entry = FileRepMirror_LookupFileName(fileName);

				if (entry != NULL)
				{
					Assert(entry->fd > 0);
					fd = entry->fd;

					switch (fileRepMessageHeader->fileRepRelationType)
					{

						case FileRepRelationTypeAppendOnly:

							Assert(fileRepMessageHeader->fileRepRelationType == entry->fileRepRelationType);

							seekResult = FileSeek(fd,
												  fileRepMessageHeader->fileRepOperationDescription.open.logicalEof,
												  SEEK_SET);
							if (seekResult != fileRepMessageHeader->fileRepOperationDescription.open.logicalEof)
							{
								status = STATUS_ERROR;
								ereport(WARNING,
										(errcode_for_file_access(),
										 errmsg("mirror failure, "
												"could not seek to file position " INT64_FORMAT " : %m, "
												"failover requested",
												fileRepMessageHeader->fileRepOperationDescription.open.logicalEof),
										 errhint("run gprecoverseg to re-establish mirror connectivity"),
										 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
														   fileRepMessageHeader->fileRepRelationType,
														   fileRepMessageHeader->fileRepOperation,
														   fileRepMessageHeader->messageCount),
										 FileRep_errcontext()));
								break;
							}
							/* no break */

						case FileRepRelationTypeFlatFile:
						case FileRepRelationTypeBufferPool:
						case FileRepRelationTypeBulk:

							found = TRUE;

							if (Debug_filerep_print)
							{
								ereport(LOG,
										(errmsg("file with file descriptor '%d' is already opened ",
												entry->fd),
										 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
														   fileRepMessageHeader->fileRepRelationType,
														   fileRepMessageHeader->fileRepOperation,
														   fileRepMessageHeader->messageCount),
										 FileRep_errcontext()));
							}

							break;

						default:
							break;
					}
					if (status != STATUS_OK || found)
					{
						break;
					}
				}

				/*
				 * special handling for Append Only in order to behave the
				 * same as primary
				 */

				if (fileRepMessageHeader->fileRepRelationType == FileRepRelationTypeAppendOnly)
				{

					fd = PathNameOpenFile(
										  fileName,
										  fileRepMessageHeader->fileRepOperationDescription.open.fileFlags,
										  fileRepMessageHeader->fileRepOperationDescription.open.fileMode);
					if (fd < 0)
					{
						if (fileRepMessageHeader->fileRepOperationDescription.open.logicalEof > 0)
						{

							status = STATUS_ERROR;

							ereport(WARNING,
									(errcode_for_file_access(),
									 errmsg("mirror failure, "
											"could not open file position:" INT64_FORMAT " file flags '%x' "
											"file mode '%o' : %m, "
											"failover requested",
											fileRepMessageHeader->fileRepOperationDescription.open.logicalEof,
											fileRepMessageHeader->fileRepOperationDescription.open.fileFlags,
											fileRepMessageHeader->fileRepOperationDescription.open.fileMode),
									 errhint("run gprecoverseg to re-establish mirror connectivity"),
									 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
													   fileRepMessageHeader->fileRepRelationType,
													   fileRepMessageHeader->fileRepOperation,
													   fileRepMessageHeader->messageCount),
									 FileRep_errcontext()));

						}
						else
						{

							fd = 0;

							if (Debug_filerep_print)
								ereport(LOG,
										(errcode_for_file_access(),
										 errmsg("could not open file, primary will send another request for create "
												"position " INT64_FORMAT " file flags '%x' "
												"file mode '%o' : %m",
												fileRepMessageHeader->fileRepOperationDescription.open.logicalEof,
												fileRepMessageHeader->fileRepOperationDescription.open.fileFlags,
												fileRepMessageHeader->fileRepOperationDescription.open.fileMode),
										 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
														   fileRepMessageHeader->fileRepRelationType,
														   fileRepMessageHeader->fileRepOperation,
														   fileRepMessageHeader->messageCount),
										 FileRep_errcontext()));
						}
						break;
					}

					seekResult = FileSeek(fd,
										  fileRepMessageHeader->fileRepOperationDescription.open.logicalEof,
										  SEEK_SET);
					if (seekResult != fileRepMessageHeader->fileRepOperationDescription.open.logicalEof)
					{
						status = STATUS_ERROR;
						ereport(WARNING,
								(errcode_for_file_access(),
								 errmsg("mirror failure, "
										"could not seek to file position " INT64_FORMAT " : %m, "
										"failover requested",
										fileRepMessageHeader->fileRepOperationDescription.open.logicalEof),
								 errhint("run gprecoverseg to re-establish mirror connectivity"),
								 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
												   fileRepMessageHeader->fileRepRelationType,
												   fileRepMessageHeader->fileRepOperation,
												   fileRepMessageHeader->messageCount),
								 FileRep_errcontext()));
						break;
					}
				}
				else
				{

					if (fileRepMessageHeader->fileRepRelationType == FileRepRelationTypeFlatFile)
					{
						errno = 0;
						fd = PathNameOpenFile(
											  fileName,
											  fileRepMessageHeader->fileRepOperationDescription.open.fileFlags,
											  fileRepMessageHeader->fileRepOperationDescription.open.fileMode);

						if (fd < 0 &&
							fileRepMessageHeader->fileRepOperationDescription.open.suppressError == TRUE &&
							errno == ENOENT)
						{
							if (Debug_filerep_print)
								ereport(LOG,
										(errcode_for_file_access(),
										 errmsg("could not open file '%s' suppressError==TRUE : %m", fileName)));

							errno = 0;
							fd = PathNameOpenFile(
												  fileName,
												  fileRepMessageHeader->fileRepOperationDescription.open.fileFlags | O_CREAT,
												  fileRepMessageHeader->fileRepOperationDescription.open.fileMode);
						}

						if (fd < 0 &&
							fileRepMessageHeader->fileRepOperationDescription.open.suppressError == TRUE &&
							errno == ENOENT)
						{
							if (Debug_filerep_print)
								ereport(LOG,
										(errcode_for_file_access(),
										 errmsg("could not open file '%s' suppressError==TRUE : %m", fileName)));
							fd = 0;
							break;
						}

						if (fd < 0)
						{
							status = STATUS_ERROR;

							ereport(WARNING,
									(errcode_for_file_access(),
									 errmsg("mirror failure, "
											"could not open file position " INT64_FORMAT " "
											"file flags '%x' file mode '%o' : %m, "
											"failover requested",
											fileRepMessageHeader->fileRepOperationDescription.open.logicalEof,
											fileRepMessageHeader->fileRepOperationDescription.open.fileFlags,
											fileRepMessageHeader->fileRepOperationDescription.open.fileMode),
									 errhint("run gprecoverseg to re-establish mirror connectivity"),
									 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
													   fileRepMessageHeader->fileRepRelationType,
													   fileRepMessageHeader->fileRepOperation,
													   fileRepMessageHeader->messageCount),
									 FileRep_errcontext()));

							break;
						}

					}
					else
					{

						errno = 0;
						fd = PathNameOpenFile(
											  fileName,
											  fileRepMessageHeader->fileRepOperationDescription.open.fileFlags,
											  fileRepMessageHeader->fileRepOperationDescription.open.fileMode);

						/*
						 * Segment Files > 0 for Buffer Pool are not created
						 * transactionally hence during in transition to
						 * Resync and crash recovery are created explicitly
						 */
						if (fd < 0 &&
							errno == ENOENT &&
							fileRepMessageHeader->fileRepRelationType == FileRepRelationTypeBufferPool &&
							fileRepMessageHeader->fileRepIdentifier.fileRepRelationIdentifier.segmentFileNum > 0)
						{
							errno = 0;
							fd = PathNameOpenFile(
												  fileName,
												  O_CREAT | fileRepMessageHeader->fileRepOperationDescription.open.fileFlags | O_RDWR | PG_BINARY,
												  fileRepMessageHeader->fileRepOperationDescription.open.fileMode);
						}

						if ((fd < 0) && ((errno == EEXIST) || (errno == ENOENT)))
						{

							if (Debug_filerep_print)
								ereport(LOG,
										(errcode_for_file_access(),
										 errmsg("could not open file position '" INT64_FORMAT "' "
												"file flags '%x' file mode '%o' : %m",
												fileRepMessageHeader->fileRepOperationDescription.open.logicalEof,
												fileRepMessageHeader->fileRepOperationDescription.open.fileFlags,
												fileRepMessageHeader->fileRepOperationDescription.open.fileMode),
										 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
														   fileRepMessageHeader->fileRepRelationType,
														   fileRepMessageHeader->fileRepOperation,
														   fileRepMessageHeader->messageCount),
										 FileRep_errcontext()));

							/*
							 * NOTE see md.c L242 there are exceptions, how
							 * mirror will know about it
							 */
							fd = 0;
							break;
						}

						if (fd < 0)
						{
							status = STATUS_ERROR;

							ereport(WARNING,
									(errcode_for_file_access(),
									 errmsg("mirror failure, "
											"could not open file position '" INT64_FORMAT "' "
											"file flags '%x' file mode '%o' : %m, "
											"failover requested",
											fileRepMessageHeader->fileRepOperationDescription.open.logicalEof,
											fileRepMessageHeader->fileRepOperationDescription.open.fileFlags,
											fileRepMessageHeader->fileRepOperationDescription.open.fileMode),
									 errhint("run gprecoverseg to re-establish mirror connectivity"),
									 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
													   fileRepMessageHeader->fileRepRelationType,
													   fileRepMessageHeader->fileRepOperation,
													   fileRepMessageHeader->messageCount),
									 FileRep_errcontext()));

							break;
						}

					}
				}
				status = FileRepMirror_InsertFileName(
													  fileName,
													  fd,
													  fileRepMessageHeader->fileRepRelationType);

				if (status != STATUS_OK)
				{
					/*
					 * File is closed explicitly since it is not inserted into
					 * hash table. If file is inserted in hash table then it
					 * is closed as part of the removing hash table during
					 * shutdown.
					 */
					FileClose(fd);
					break;
				}

				break;

			case FileRepOperationWrite:

				fd = FileRepMirror_GetFileDescriptor(fileName);

				if (fd < 0)
				{
					if (fileRepMessageHeader->fileRepRelationType == FileRepRelationTypeAppendOnly)
					{

						ereport(WARNING,
								(errmsg("mirror failure, "
										"could not find file descriptor, "
										"failover requested"),
								 errhint("run gprecoverseg to re-establish mirror connectivity"),
								 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
												   fileRepMessageHeader->fileRepRelationType,
												   fileRepMessageHeader->fileRepOperation,
												   fileRepMessageHeader->messageCount),
								 FileRep_errcontext()));

						status = STATUS_ERROR;
						break;
					}

					errno = 0;
					fd = PathNameOpenFile(
										  fileName,
										  O_RDWR,
										  fileMode);


					/*
					 * Segment Files > 0 for Buffer Pool are not created
					 * transactionally hence during in transition to Resync
					 * and crash recovery are created explicitly
					 */
					if (fd < 0 &&
						errno == ENOENT &&
						dataState == DataStateInResync &&
						fileRepMessageHeader->fileRepRelationType == FileRepRelationTypeBufferPool &&
						fileRepMessageHeader->fileRepIdentifier.fileRepRelationIdentifier.segmentFileNum > 0)
					{
						errno = 0;
						fd = PathNameOpenFile(
											  fileName,
											  O_CREAT | O_RDWR | PG_BINARY,
											  fileMode);
					}

					if (fd < 0)
					{
						status = STATUS_ERROR;
						ereport(WARNING,
								(errcode_for_file_access(),
								 errmsg("mirror failure, "
										"could not open file file flags '%x' file mode '%o': %m, "
										"failover requested",
										O_CREAT | O_RDWR | PG_BINARY,
										fileMode),
								 errhint("run gprecoverseg to re-establish mirror connectivity"),
								 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
												   fileRepMessageHeader->fileRepRelationType,
												   fileRepMessageHeader->fileRepOperation,
												   fileRepMessageHeader->messageCount),
								 FileRep_errcontext()));
						break;
					}

					status = FileRepMirror_InsertFileName(
														  fileName,
														  fd,
														  fileRepMessageHeader->fileRepRelationType);

					if (status != STATUS_OK)
					{
						/*
						 * File is closed explicitly since it is not inserted
						 * into hash table. If file is inserted in hash table
						 * then it is closed as part of the removing hash
						 * table during shutdown.
						 */
						FileClose(fd);
						break;
					}
				}

				/*
				 * Segment Files > 0 for Buffer Pool are not created
				 * transactionally hence during in transition to Resync and
				 * crash recovery are created explicitly
				 */
				if (fd > 0 &&
					dataState == DataStateInResync &&
					fileRepMessageHeader->fileRepRelationType == FileRepRelationTypeBufferPool &&
					fileRepMessageHeader->fileRepIdentifier.fileRepRelationIdentifier.segmentFileNum > 0)
				{
					Assert(fileRepMessageHeader->fileRepOperationDescription.write.offset != FILEREP_OFFSET_UNDEFINED);

					status = SegmentFileFillZero(fd,
												 (int64) fileRepMessageHeader->fileRepOperationDescription.write.offset);

					if (status != STATUS_OK)
					{
						ereport(WARNING,
								(errcode_for_file_access(),
								 errmsg("mirror failure, "
										"could not fill zero blocks into segment file : %m, "
										"failover requested"),
								 errhint("run gprecoverseg to re-establish mirror connectivity"),
								 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
												   fileRepMessageHeader->fileRepRelationType,
												   fileRepMessageHeader->fileRepOperation,
												   fileRepMessageHeader->messageCount),
								 FileRep_errcontext()));

						break;
					}
				}

				if (fileRepMessageHeader->fileRepOperationDescription.write.offset != FILEREP_OFFSET_UNDEFINED)
				{
					seekResult = FileSeek(fd,
										  fileRepMessageHeader->fileRepOperationDescription.write.offset,
										  SEEK_SET);
					if (seekResult != fileRepMessageHeader->fileRepOperationDescription.write.offset)
					{
						status = STATUS_ERROR;
						ereport(WARNING,
								(errcode_for_file_access(),
								 errmsg("mirror failure, "
										"could not seek to file position '%d' : %m, "
										"failover requested",
										fileRepMessageHeader->fileRepOperationDescription.write.offset),
								 errhint("run gprecoverseg to re-establish mirror connectivity"),
								 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
												   fileRepMessageHeader->fileRepRelationType,
												   fileRepMessageHeader->fileRepOperation,
												   fileRepMessageHeader->messageCount),
								 FileRep_errcontext()));
						break;

					}
				}

				errno = 0;

				if (gp_heap_verify_checksums_on_mirror)
				{
					if (fileRepMessageHeader->fileRepRelationType == FileRepRelationTypeBufferPool)
					{
						BlockNumber blkno = (BlockNumber) (
														   fileRepMessageHeader->fileRepOperationDescription.write.offset / BLCKSZ);
						bool		isBufferPoolChecksumCorrect = PageIsVerified(fileRepMessageBody, blkno);

						if (!isBufferPoolChecksumCorrect)
						{
							PageHeader	header = (PageHeader) fileRepMessageBody;
							RelFileNode relfilenode = fileRepMessageHeader->fileRepIdentifier.fileRepRelationIdentifier.relFileNode;

							ereport(ERROR,
									(errmsg("Heap Checksum for relfilenode %d/%d/%d is inconsistent for blkno %d "
											"with checksum %d from primary, suspect data corruption.",
											relfilenode.spcNode, relfilenode.dbNode, relfilenode.relNode,
											blkno,
											header->pd_checksum),
									 errprintstack(true)));
						}
					}
				}

				if ((int) FileWrite(
									fd,
									fileRepMessageBody,
									fileRepMessageHeader->fileRepOperationDescription.write.dataLength) !=
					fileRepMessageHeader->fileRepOperationDescription.write.dataLength)
				{

					/*
					 * if write didn't set errno, assume problem is no disk
					 * space
					 */
					if (errno == 0)
						errno = ENOSPC;

					status = STATUS_ERROR;
					ereport(WARNING,
							(errcode_for_file_access(),
							 errmsg("mirror failure, "
									"could not write to file : %m, "
									"failover requested"),
							 errhint("run gprecoverseg to re-establish mirror connectivity"),
							 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
											   fileRepMessageHeader->fileRepRelationType,
											   fileRepMessageHeader->fileRepOperation,
											   fileRepMessageHeader->messageCount),
							 FileRep_errcontext()));

					break;
				}

				break;
			case FileRepOperationFlush:

				fd = FileRepMirror_GetFileDescriptor(fileName);

				if (fd < 0)
				{
					if (fileRepMessageHeader->fileRepRelationType == FileRepRelationTypeAppendOnly)
					{

						ereport(WARNING,
								(errcode_for_file_access(),
								 errmsg("mirror failure, "
										"could not find file descriptor : %m, "
										"failover requested"),
								 errhint("run gprecoverseg to re-establish mirror connectivity"),
								 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
												   fileRepMessageHeader->fileRepRelationType,
												   fileRepMessageHeader->fileRepOperation,
												   fileRepMessageHeader->messageCount),
								 FileRep_errcontext()));

						status = STATUS_ERROR;
						break;
					}

					errno = 0;
					fd = PathNameOpenFile(
										  fileName,
										  O_RDWR,
										  fileMode);
					if (fd < 0 && errno == ENOENT)
					{
						fd = 0;
						if (Debug_filerep_print)
							ereport(LOG,
									(errcode_for_file_access(),
									 errmsg("could not open file, "
											"file was already dropped identifier '%s' : %m",
											(fileName == NULL) ? "" : fileName)));
						break;
					}

					if (fd < 0)
					{
						status = STATUS_ERROR;
						ereport(WARNING,
								(errcode_for_file_access(),
								 errmsg("mirror failure, "
										"could not find file descriptor or open file : %m"
										"failover requested"),
								 errhint("run gprecoverseg to re-establish mirror connectivity"),
								 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
												   fileRepMessageHeader->fileRepRelationType,
												   fileRepMessageHeader->fileRepOperation,
												   fileRepMessageHeader->messageCount),
								 FileRep_errcontext()));

						break;
					}

					status = FileRepMirror_InsertFileName(
														  fileName,
														  fd,
														  fileRepMessageHeader->fileRepRelationType);

					if (status != STATUS_OK)
					{
						/*
						 * File is closed explicitly since it is not inserted
						 * into hash table. If file is inserted in hash table
						 * then it is closed as part of the removing hash
						 * table during shutdown.
						 */
						FileClose(fd);
						break;
					}
				}

				SIMPLE_FAULT_INJECTOR(FileRepFlush);

				gettimeofday(&currentTime, NULL);
				beginTime = (pg_time_t) currentTime.tv_sec;

				status = FileSync(fd);

				gettimeofday(&currentTime, NULL);
				endTime = (pg_time_t) currentTime.tv_sec;

				if (endTime - beginTime > 120)
					/* 120 seconds */
				{
					ereport(LOG,
							(errmsg("fsync was taken '" INT64_FORMAT "' seconds, "
									"mirror may not be able to keep up with primary, "
									"primary may transition to change tracking",
									endTime - beginTime),
							 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
											   fileRepMessageHeader->fileRepRelationType,
											   fileRepMessageHeader->fileRepOperation,
											   fileRepMessageHeader->messageCount)));
				}

				if (endTime - beginTime > 60)
					/* 60 seconds */
				{

					{
						char		tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];

						snprintf(tmpBuf, sizeof(tmpBuf), "fsync was taken " INT64_FORMAT " seconds identifier '%s' ",
								 endTime - beginTime,
								 fileName);

						FileRep_InsertConfigLogEntry(tmpBuf);
					}
				}

				if (status != STATUS_OK)
				{
					ereport(WARNING,
							(errcode_for_file_access(),
							 errmsg("mirror failure, "
									"could not fsync file : %m"
									"failover requested"),
							 errhint("run gprecoverseg to re-establish mirror connectivity"),
							 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
											   fileRepMessageHeader->fileRepRelationType,
											   fileRepMessageHeader->fileRepOperation,
											   fileRepMessageHeader->messageCount),
							 FileRep_errcontext()));

					break;
				}

				break;

			case FileRepOperationClose:

				fd = FileRepMirror_GetFileDescriptor(fileName);

				if (fd < 0)
				{

					if (Debug_filerep_print)
						ereport(LOG,
								(errmsg("could not find file descriptor"),
								 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
												   fileRepMessageHeader->fileRepRelationType,
												   fileRepMessageHeader->fileRepOperation,
												   fileRepMessageHeader->messageCount),
								 FileRep_errcontext()));

					fd = 0;
					break;
				}

				FileClose(fd);

				FileRepMirror_RemoveFileName(fileName);

				break;

			case FileRepOperationFlushAndClose:

				fd = FileRepMirror_GetFileDescriptor(fileName);

				if (fd < 0)
				{
					/*
					 * ereport(LOG, (errcode_for_file_access(), errmsg("could
					 * not find file descriptor \"%s\": %m", fileName)));
					 * status = STATUS_ERROR; break;
					 */

					errno = 0;
					fd = PathNameOpenFile(
										  fileName,
										  O_RDWR,
										  fileMode);


					if (fd < 0 && errno == ENOENT)
					{
						fd = 0;

						ereport(LOG,
								(errcode_for_file_access(),
								 errmsg("could not flush and close file, "
										"file was already dropped identifier '%s' : %m",
										(fileName == NULL) ? "<null>" : fileName),
								 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
												   fileRepMessageHeader->fileRepRelationType,
												   fileRepMessageHeader->fileRepOperation,
												   fileRepMessageHeader->messageCount),
								 FileRep_errcontext()));
						break;
					}

					if (fd < 0)
					{
						status = STATUS_ERROR;
						ereport(WARNING,
								(errcode_for_file_access(),
								 errmsg("mirror failure, "
										"could not find file descriptor or open file : %m"
										"failover requested"),
								 errhint("run gprecoverseg to re-establish mirror connectivity"),
								 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
												   fileRepMessageHeader->fileRepRelationType,
												   fileRepMessageHeader->fileRepOperation,
												   fileRepMessageHeader->messageCount),
								 FileRep_errcontext()));

						break;
					}

					status = FileRepMirror_InsertFileName(
														  fileName,
														  fd,
														  fileRepMessageHeader->fileRepRelationType);

					if (status != STATUS_OK)
					{
						/*
						 * File is closed explicitly since it is not inserted
						 * into hash table. If file is inserted in hash table
						 * then it is closed as part of the removing hash
						 * table during shutdown.
						 */
						FileClose(fd);
						break;
					}
				}

				status = FileSync(fd);

				if (status != STATUS_OK)
				{
					ereport(WARNING,
							(errcode_for_file_access(),
							 errmsg("mirror failure, "
									"could not fsync file : %m"
									"failover requested"),
							 errhint("run gprecoverseg to re-establish mirror connectivity"),
							 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
											   fileRepMessageHeader->fileRepRelationType,
											   fileRepMessageHeader->fileRepOperation,
											   fileRepMessageHeader->messageCount),
							 FileRep_errcontext()));

					/* no break */
				}

				FileClose(fd);

				FileRepMirror_RemoveFileName(fileName);

				break;


			case FileRepOperationTruncate:

				fd = FileRepMirror_GetFileDescriptor(fileName);

				if (fd < 0)
				{
					/*
					 * ereport(LOG, (errcode_for_file_access(), errmsg("could
					 * not find file descriptor \"%s\": %m", fileName)));
					 * status = STATUS_ERROR; break;
					 */

					errno = 0;
					fd = PathNameOpenFile(
										  fileName,
										  O_RDWR,
										  fileMode);

					if (fd < 0)
					{
						if (errno == ENOENT)
						{
							{
								char		tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];

								snprintf(tmpBuf, sizeof(tmpBuf),
										 "could not find '%s' operation '%s' position '" INT64_FORMAT "' ",
										 fileName,
										 FileRepOperationToString[fileRepMessageHeader->fileRepOperation],
										 fileRepMessageHeader->fileRepOperationDescription.truncate.position);

								FileRep_InsertConfigLogEntry(tmpBuf);
							}

							break;
						}

						status = STATUS_ERROR;
						ereport(WARNING,
								(errcode_for_file_access(),
								 errmsg("mirror failure, "
										"could not find file descriptor or open file : %m"
										"failover requested"),
								 errhint("run gprecoverseg to re-establish mirror connectivity"),
								 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
												   fileRepMessageHeader->fileRepRelationType,
												   fileRepMessageHeader->fileRepOperation,
												   fileRepMessageHeader->messageCount),
								 FileRep_errcontext()));

						break;
					}

					status = FileRepMirror_InsertFileName(
														  fileName,
														  fd,
														  fileRepMessageHeader->fileRepRelationType);

					if (status != STATUS_OK)
					{
						/*
						 * File is closed explicitly since it is not inserted
						 * into hash table. If file is inserted in hash table
						 * then it is closed as part of the removing hash
						 * table during shutdown.
						 */
						FileClose(fd);
						break;
					}

				}

				status = FileTruncate(fd, fileRepMessageHeader->fileRepOperationDescription.truncate.position);

				if (status < 0)
				{
					ereport(WARNING,
							(errcode_for_file_access(),
							 errmsg("mirror failure, "
									"could not truncate file to position '" INT64_FORMAT "' : %m"
									"failover requested",
									fileRepMessageHeader->fileRepOperationDescription.truncate.position),
							 errhint("run gprecoverseg to re-establish mirror connectivity"),
							 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
											   fileRepMessageHeader->fileRepRelationType,
											   fileRepMessageHeader->fileRepOperation,
											   fileRepMessageHeader->messageCount),
							 FileRep_errcontext()));

					status = STATUS_ERROR;
					break;
				}

				if (fileRepMessageHeader->fileRepRelationType == FileRepRelationTypeBufferPool)
				{
					FileName	fileNameLocal = NULL;
					int			segmentFileNumLocal = fileRepMessageHeader->fileRepIdentifier.fileRepRelationIdentifier.segmentFileNum;

					fileRepMessageHeader->fileRepIdentifier.fileRepRelationIdentifier.segmentFileNum = 0;

					fileNameLocal = FileRep_GetFileName(
														fileRepMessageHeader->fileRepIdentifier,
														fileRepMessageHeader->fileRepRelationType);

					fileRepMessageHeader->fileRepIdentifier.fileRepRelationIdentifier.segmentFileNum = segmentFileNumLocal;

					status = FileRepMirror_DropBufferPoolSegmentFiles(fileNameLocal,
																	  segmentFileNumLocal);
					if (fileNameLocal)
					{
						pfree(fileNameLocal);
						fileNameLocal = NULL;
					}
				}

				break;

			case FileRepOperationReconcileXLogEof:

				status = XLogReconcileEofMirror(
												fileRepMessageHeader->fileRepOperationDescription.reconcile.xLogEof,
												&mirrorEofRecPtr);

				if (status != STATUS_OK)
				{
					ereport(WARNING,
							(errmsg("mirror failure, "
									"could not reconcile xlog file"
									"failover requested"),
							 errhint("run gprecoverseg to re-establish mirror connectivity"),
							 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
											   fileRepMessageHeader->fileRepRelationType,
											   fileRepMessageHeader->fileRepOperation,
											   fileRepMessageHeader->messageCount),
							 FileRep_errcontext()));

					break;
				}

				fileRepMessageHeader->fileRepOperationDescription.reconcile.xLogEof = mirrorEofRecPtr;

				{
					char		tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];

					snprintf(tmpBuf, sizeof(tmpBuf), "xlog reconcile identifier '%s' lsn xlog '%s(%u/%u)' ",
							 fileName,
							 XLogLocationToString(&mirrorEofRecPtr),
							 mirrorEofRecPtr.xlogid,
							 mirrorEofRecPtr.xrecoff);

					FileRep_InsertConfigLogEntry(tmpBuf);
				}

				break;

			case FileRepOperationCreate:

				switch (fileRepMessageHeader->fileRepRelationType)
				{
					case FileRepRelationTypeDir:

						/*
						 * when tablespaces are added mkdir() creates only one
						 * sub-directory
						 */
						errno = 0;
						status = mkdir(fileName, 0700);

						if (errno == EEXIST &&
							fileRepMessageHeader->fileRepOperationDescription.create.ignoreAlreadyExists)
						{
							status = STATUS_OK;
							errno = 0;
						}

						if (status < 0)
						{
							/*
							 * status == STATUS_OK then segment does not
							 * transition into Fault, instead error is
							 * reported to primary
							 *
							 * status == STATUS_ERROR then segment transitions
							 * into Fault
							 */
							if (fileRepMessageHeader->fileRepOperationDescription.create.ignoreAlreadyExists)
							{
								status = STATUS_ERROR;
								ereport(WARNING,
										(errcode_for_file_access(),
										 errmsg("mirror failure, "
												"could not create directory : %m"
												"failover requested"),
										 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
														   fileRepMessageHeader->fileRepRelationType,
														   fileRepMessageHeader->fileRepOperation,
														   fileRepMessageHeader->messageCount),
										 FileRep_errcontext()));
							}
							else
							{
								status = STATUS_OK;

								if (Debug_filerep_print)
									ereport(LOG,
											(errcode_for_file_access(),
											 errmsg("could not create directory : %m"),
											 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
															   fileRepMessageHeader->fileRepRelationType,
															   fileRepMessageHeader->fileRepOperation,
															   fileRepMessageHeader->messageCount),
											 FileRep_errcontext()));
							}
						}

						switch (errno)
						{
							case 0:
								fileRepMessageHeader->fileRepOperationDescription.create.mirrorStatus = FileRepStatusSuccess;
								break;

							case EEXIST:
								fileRepMessageHeader->fileRepOperationDescription.create.mirrorStatus = FileRepStatusDirectoryExist;
								break;

							case EACCES:
								fileRepMessageHeader->fileRepOperationDescription.create.mirrorStatus = FileRepStatusNoPermissions;
								break;

							case ENOENT:
								fileRepMessageHeader->fileRepOperationDescription.create.mirrorStatus = FileRepStatusNoSuchFileOrDirectory;
								break;

							case ENOTDIR:
								fileRepMessageHeader->fileRepOperationDescription.create.mirrorStatus = FileRepStatusNotDirectory;
								break;

							case EROFS:
								fileRepMessageHeader->fileRepOperationDescription.create.mirrorStatus = FileRepStatusReadOnlyFileSystem;
								break;

							case ENOSPC:
								fileRepMessageHeader->fileRepOperationDescription.create.mirrorStatus = FileRepStatusNoSpace;
								break;

							default:
								fileRepMessageHeader->fileRepOperationDescription.create.mirrorStatus = FileRepStatusMirrorError;
								break;
						}

						break;

					case FileRepRelationTypeFlatFile:

						fd = PathNameOpenFile(
											  fileName,
											  O_RDWR | O_CREAT | O_EXCL | PG_BINARY,
											  fileMode);

						if (fd < 0 && errno == EEXIST)
						{
							fd = PathNameOpenFile(
												  fileName,
												  O_RDWR | PG_BINARY,
												  fileMode);

						}

						if (fd < 0)
						{
							status = STATUS_ERROR;
							ereport(WARNING,
									(errcode_for_file_access(),
									 errmsg("mirror failure, "
											"could not create file : %m, "
											"failover requested"),
									 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
													   fileRepMessageHeader->fileRepRelationType,
													   fileRepMessageHeader->fileRepOperation,
													   fileRepMessageHeader->messageCount),
									 FileRep_errcontext()));

							break;
						}

						/*
						 * The file is closed after create. Open operation is
						 * required to open file for write.
						 */
						status = FileRepMirror_InsertFileName(
															  fileName,
															  fd,
															  fileRepMessageHeader->fileRepRelationType);

						if (status != STATUS_OK)
						{
							/*
							 * File is closed explicitly since it is not
							 * inserted into hash table. If file is inserted
							 * in hash table then it is closed as part of the
							 * removing hash table during shutdown.
							 */
							FileClose(fd);
						}
						break;

					default:
						Assert(0);
				}

				break;

			case FileRepOperationDrop:

				switch (fileRepMessageHeader->fileRepRelationType)
				{
					case FileRepRelationTypeDir:

						errno = 0;
						if (rmtree(fileName, true) == FALSE && errno != ENOENT)
						{

							status = STATUS_ERROR;

							ereport(WARNING,
									(errcode_for_file_access(),
									 errmsg("mirror failure, "
											"could not remove directory : %m, "
											"failover requested"),
									 errhint("run gprecoverseg to re-establish mirror connectivity"),
									 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
													   fileRepMessageHeader->fileRepRelationType,
													   fileRepMessageHeader->fileRepOperation,
													   fileRepMessageHeader->messageCount),
									 FileRep_errcontext()));

							break;
						}

						break;

					case FileRepRelationTypeBufferPool:
						/* Buffer Pool drops segmentFileNum = 0 */
						status = FileRepMirror_DropBufferPoolSegmentFiles(fileName, 0);
						if (status != STATUS_OK)
						{
							break;
						}
						/* NO BREAK */
					case FileRepRelationTypeAppendOnly:
					case FileRepRelationTypeFlatFile:

						status = FileRepMirror_Drop(fileName);
						break;

					default:
						Assert(0);
				}

				break;

			case FileRepOperationDropTemporaryFiles:

				MirroredFlatFile_DropTemporaryFiles();
				break;

			case FileRepOperationDropFilesFromDir:

				switch (fileRepMessageHeader->fileRepRelationType)
				{
					case FileRepRelationTypeFlatFile:
						FileRepMirror_DropFilesFromDir(fileName);
						break;

					default:
						break;
				}
				break;

			case FileRepOperationValidation:

				switch (fileRepMessageHeader->fileRepOperationDescription.validation.fileRepValidationType)
				{
					case FileRepValidationFilespace:

						Assert(fileRepMessageHeader->fileRepRelationType == FileRepRelationTypeDir);

						fileRepMessageHeader->fileRepOperationDescription.validation.mirrorStatus = FileRepMirror_Validation(fileName);
						break;

					case FileRepValidationExistence:
						{
							struct stat st;

							if (stat(fileName, &st) >= 0)
							{
								fileRepMessageHeader->fileRepOperationDescription.validation.mirrorStatus = FileRepStatusSuccess;
							}
							else
							{
								fileRepMessageHeader->fileRepOperationDescription.validation.mirrorStatus = FileRepStatusNoSuchFileOrDirectory;
							}

							break;
						}
					default:
						Assert(0);
						break;
				}

				break;

			case FileRepOperationRename:

				errno = 0;

				/* Ensure that file is not opened. */
				fd = FileRepMirror_GetFileDescriptor(fileName);

				if (fd > 0)
				{
					if (Debug_filerep_print)
						ereport(LOG,
								(errmsg("FileRepOperationRename() removing file name from table. fd '%d' ",
										fd),
								 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
												   fileRepMessageHeader->fileRepRelationType,
												   fileRepMessageHeader->fileRepOperation,
												   fileRepMessageHeader->messageCount),
								 FileRep_errcontext()));

					/* FileClose(fd); */
					FileRepMirror_RemoveFileName(fileName);
				}

				newFileName = FileRep_GetFileName(
												  fileRepMessageHeader->fileRepOperationDescription.rename.fileRepIdentifier,
												  fileRepMessageHeader->fileRepRelationType);

				/* REMOVE after fix in Open */

				fd = FileRepMirror_GetFileDescriptor(newFileName);

				if (fd > 0)
				{
					if (Debug_filerep_print)
						ereport(LOG,
								(errmsg("FileRepOperationRename() new identifier '%s' fd '%d' ",
										newFileName,
										fd),
								 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
												   fileRepMessageHeader->fileRepRelationType,
												   fileRepMessageHeader->fileRepOperation,
												   fileRepMessageHeader->messageCount),
								 FileRep_errcontext()));

					SetDeleteOnExit(fd);
					FileClose(fd);
					FileRepMirror_RemoveFileName(newFileName);
				}
				fd = 0;

				/****/


				/* XXX: hacky, I know. By damn, we need to ship 4.0 some day */
				if (strcmp(newFileName, "global/pg_database") == 0 ||
					strcmp(newFileName, "global/pg_auth") == 0 ||
					strcmp(newFileName, "global/pg_auth_time_constraint") == 0)
				{

					errno = 0;
					if (rename(fileName, newFileName) < 0 && errno != ENOENT)
					{
						status = STATUS_ERROR;

						ereport(WARNING,
								(errcode_for_file_access(),
								 errmsg("mirror failure, "
										"could not rename to '%s' : %m, "
										"failover requested",
										newFileName),
								 errhint("run gprecoverseg to re-establish mirror connectivity"),
								 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
												   fileRepMessageHeader->fileRepRelationType,
												   fileRepMessageHeader->fileRepOperation,
												   fileRepMessageHeader->messageCount),
								 FileRep_errcontext()));

						break;
					}
				}
				else
				{
					/*
					 * files in pg_xlog may not be recycled due to reversing
					 * of primary/mirror roles
					 */
					if (unlink(newFileName) == 0)
					{
						char		tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];

						snprintf(tmpBuf, sizeof(tmpBuf), "unlink identifier '%s' ",
								 newFileName);

						FileRep_InsertConfigLogEntry(tmpBuf);
					}

					errno = 0;
					if (link(fileName, newFileName) < 0 && errno != ENOENT)
					{
						status = STATUS_ERROR;

						ereport(WARNING,
								(errcode_for_file_access(),
								 errmsg("mirror failure, "
										"could not link to '%s' : %m, "
										"failover requested",
										newFileName),
								 errhint("run gprecoverseg to re-establish mirror connectivity"),
								 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
												   fileRepMessageHeader->fileRepRelationType,
												   fileRepMessageHeader->fileRepOperation,
												   fileRepMessageHeader->messageCount),
								 FileRep_errcontext()));
						break;

					}
				}
				if (newFileName)
				{
					pfree(newFileName);
					newFileName = NULL;
				}

				break;
			case FileRepOperationShutdown:
				/* NoOp. Just send ACK back. */
				break;

			case FileRepOperationInSyncTransition:

				FileRep_SetDataState(DataStateInSync, TRUE);

				FileRep_SetSegmentState(SegmentStateReady, FaultTypeNotInitialized);

				primaryMirrorSetInSync();

				break;

			case FileRepOperationHeartBeat:
				/* NoOp. Just send ACK back. */
				break;

			case FileRepOperationStartSlruChecksum:
				Assert(fileRepMessageHeader->fileRepRelationType == FileRepRelationTypeFlatFile);

				ereport(LOG,
						(errmsg("mirror beginning SLRU checksum file creation, dir: %s",
								fileRepMessageHeader->fileRepIdentifier.fileRepFlatFileIdentifier.directorySimpleName)));

				if (SlruCreateChecksumFile(
										   fileRepMessageHeader->fileRepIdentifier.fileRepFlatFileIdentifier.directorySimpleName) < 0)
				{
					fileRepMessageHeader->fileRepOperationDescription.startChecksum.mirrorStatus
						= FileRepStatusSlruChecksumFailed;
				}

				break;

			case FileRepOperationVerifySlruDirectoryChecksum:
				Assert(fileRepMessageHeader->fileRepRelationType == FileRepRelationTypeFlatFile);

				ereport(LOG,
						(errmsg("mirror beginning checksum comparison, dir: %s, checksum file: %s",
								fileRepMessageHeader->fileRepIdentifier.fileRepFlatFileIdentifier.directorySimpleName,
								fileRepMessageHeader->fileRepIdentifier.fileRepFlatFileIdentifier.fileSimpleName)));

				if (SlruMirrorVerifyDirectoryChecksum(
													  fileRepMessageHeader->fileRepIdentifier.fileRepFlatFileIdentifier.directorySimpleName,
													  fileRepMessageHeader->fileRepIdentifier.fileRepFlatFileIdentifier.fileSimpleName,
													  fileRepMessageHeader->fileRepOperationDescription.verifyDirectoryChecksum.md5) < 0)
				{
					fileRepMessageHeader->fileRepOperationDescription.verifyDirectoryChecksum.mirrorStatus
						= FileRepStatusSlruChecksumFailed;
				}

				break;

			default:
				Assert(0);
				break;
		}

		if (FileRep_IsOperationSynchronous(
										   fileRepMessageHeader->fileRepOperation))
		{
			if (status != STATUS_OK)
			{
				fileRepMessageHeader->fileRepAckState = FileRepAckStateMirrorInFault;
			}
			else
			{
				fileRepMessageHeader->fileRepAckState = FileRepAckStateCompleted;
			}

			if (fileRepMessageHeader->messageBodyLength)
				Assert(responseData != NULL);

			if (FileRepAckMirror_Ack(
									 fileRepMessageHeader->fileRepIdentifier,
									 fileRepMessageHeader->fileRepRelationType,
									 fileRepMessageHeader->fileRepOperation,
									 fileRepMessageHeader->fileRepOperationDescription,
									 fileRepMessageHeader->fileRepAckState,
									 fileRepMessageHeader->messageBodyLength,
									 (char *) responseData) != STATUS_OK)
			{
				status = STATUS_ERROR;

				ereport(WARNING,
						(errmsg("mirror failure, "
								"could not queue ack message to be sent to primary, "
								"failover requested"),
						 errhint("run gprecoverseg to re-establish mirror connectivity"),
						 FileRep_errdetail(fileRepMessageHeader->fileRepIdentifier,
										   fileRepMessageHeader->fileRepRelationType,
										   fileRepMessageHeader->fileRepOperation,
										   fileRepMessageHeader->messageCount),
						 FileRep_errdetail_ShmemAck(),
						 FileRep_errcontext()));

				FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
			}
		}

		if (status != STATUS_OK)
		{
			break;
		}

		if (fileName)
		{
			pfree(fileName);
			fileName = NULL;
		}

		movePositionConsume = TRUE;
	} //while (1)

		if (fileName)
		{
			pfree(fileName);
			fileName = NULL;
		}

	return status;
}

/*
 * Find File Descriptor in hash table of open files.
 */
static File
FileRepMirror_GetFileDescriptor(FileName fileName)
{
	OpenFileEntry *entry;
	File		fd;
	char		key[MAXPGPATH + 1];

	snprintf(key, sizeof(key), "%s", fileName);



	Assert(openFilesTable != NULL);

	entry = (OpenFileEntry *) hash_search(
										  openFilesTable,
										  (void *) &key,
										  HASH_FIND,
										  NULL);

	if (entry == NULL)
	{
		fd = -1;
	}
	else
	{
		fd = entry->fd;
	}

	return fd;
}

static OpenFileEntry *
FileRepMirror_LookupFileName(FileName fileName)
{
	OpenFileEntry *entry;
	char		key[MAXPGPATH + 1];

	snprintf(key, sizeof(key), "%s", fileName);

	Assert(openFilesTable != NULL);

	entry = (OpenFileEntry *) hash_search(
										  openFilesTable,
										  (void *) &key,
										  HASH_FIND,
										  NULL);

	return entry;
}

/*
 *
 *
 * fileName is relative path to $PGDATA directory
 */
static int
FileRepMirror_InsertFileName(
							 FileName fileName,
							 File fd,
							 FileRepRelationType_e fileRepRelationType)
{
	int			status = STATUS_OK;

	bool		foundPtr;
	OpenFileEntry *entry;
	char		key[MAXPGPATH + 1];

	snprintf(key, sizeof(key), "%s", fileName);


	Assert(openFilesTable != NULL);

	entry = (OpenFileEntry *) hash_search(
										  openFilesTable,
										  (void *) &key,
										  HASH_ENTER,
										  &foundPtr);

	if (entry == NULL)
	{

		ereport(WARNING,
				(errmsg("mirror failure, "
						"could not insert file information in open files hash table identifier '%s' fd '%d', "
						"failover requested",
						fileName,
						fd),
				 errhint("run gprecoverseg to re-establish mirror connectivity"),
				 FileRep_errdetail_Shmem(),
				 FileRep_errcontext()));

		status = STATUS_ERROR;
		return status;
	}

	if (foundPtr)
	{
		switch (fileRepRelationType)
		{
			case FileRepRelationTypeFlatFile:
			case FileRepRelationTypeBufferPool:

				Assert(entry->fd == fd);
				Assert(entry->fileRepRelationType == fileRepRelationType);
				entry->reference++;

				if (Debug_filerep_print)
					ereport(LOG,
							(errmsg("could not insert file information into hash table, "
									"entry is already inserted "
									"identifier '%s' file descriptor '%d' relation type '%s' ",
									fileName,
									fd,
									FileRepRelationTypeToString[fileRepRelationType])));

				break;
			case FileRepRelationTypeAppendOnly:
			case FileRepRelationTypeBulk:

				ereport(WARNING,
						(errmsg("mirror failure, "
								"could not insert file information in open files hash table, entry exists "
								"identifier '%s' fd '%d' relation type '%s', "
								"failover requested",
								fileName,
								fd,
								FileRepRelationTypeToString[fileRepRelationType]),
						 errhint("run gprecoverseg to re-establish mirror connectivity"),
						 FileRep_errdetail_Shmem(),
						 FileRep_errcontext()));

				status = STATUS_ERROR;

				break;
			default:
				break;
		}

	}
	else
	{
		entry->fd = fd;
		entry->fileRepRelationType = fileRepRelationType;
		entry->reference = 1;
	}

	return status;
}

/*
 *
 *
 * fileName is relative path from $PGDATA directory
 */
static void
FileRepMirror_RemoveFileName(FileName fileName)
{
	/* NOTE check behavior when two users have concurrently open files */

	/* NOTE make it atomic when thread pool is introduced */

	OpenFileEntry *entry;
	char		key[MAXPGPATH + 1];

	snprintf(key, sizeof(key), "%s", fileName);

	Assert(openFilesTable != NULL);

	entry = (OpenFileEntry *) hash_search(
										  openFilesTable,
										  (void *) &key,
										  HASH_FIND,
										  NULL);
	Assert(entry != NULL);

	if (entry->reference > 1)
	{
		entry->reference--;
	}
	else
	{
		/* File should be closed. */
		/* Assert(FileIsNotOpen(entry->fd));  Not public routine. */
		hash_search(openFilesTable, (void *) &key, HASH_REMOVE, NULL);
	}
	return;
}

/*
 * See checkpath2(path) in commands/filespace.c for comments.
 */
static int
FileRepMirror_Validation(FileName fileName)
{
	int			mirrorStatus = FileRepStatusSuccess;
	char	   *parentdir = NULL;
	char	   *errordir = NULL;
	struct stat st;

	errno = 0;
	/* Does the path exist? */
	if (stat(fileName, &st) >= 0)
	{
		mirrorStatus = FileRepStatusDirectoryExist;
		errordir = fileName;
		goto exit;
	}

	/* path does not exist, run checks on parent */
	parentdir = pstrdup(fileName);
	get_parent_directory(parentdir);

	/* Does the parent directory exist? */
	if (stat(parentdir, &st) < 0)
	{
		mirrorStatus = FileRepStatusNoSuchFileOrDirectory;
		errordir = parentdir;
		goto exit;
	}

	/* Is it actually a directory? */
	if (!S_ISDIR(st.st_mode))
	{
		mirrorStatus = FileRepStatusNotDirectory;
		errordir = parentdir;
		goto exit;
	}

	/* Check write permissions */
	if (access(parentdir, W_OK | X_OK) < 0)
	{
		mirrorStatus = FileRepStatusNoPermissions;
		errordir = fileName;
		goto exit;
	}

exit:
	if (mirrorStatus != FileRepStatusSuccess)
	{
		Assert(errordir != NULL);

		{
			char		tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];

			snprintf(tmpBuf, sizeof(tmpBuf), "mirror validation failure identifier '%s' : '%s' ",
					 errordir,
					 FileRepStatusToString[mirrorStatus]);

			FileRep_InsertConfigLogEntry(tmpBuf);
		}
	}

	if (parentdir)
	{
		pfree(parentdir);
	}
	return mirrorStatus;
}

static void
FileRepMirror_DropFilesFromDir(FileName fileName)
{
	DIR		   *dir = NULL;
	struct dirent *de;
	struct stat st;
	char		path[MAXPGPATH + 1];

	errno = 0;
	/* Does the path exist? */
	if (stat(fileName, &st) >= 0)
	{
		/* Is it actually a directory? */
		if (!S_ISDIR(st.st_mode))
		{
			{
				char		tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];

				snprintf(tmpBuf, sizeof(tmpBuf), "not a directory identifier '%s' errno '%d' ",
						 fileName,
						 errno);

				FileRep_InsertConfigLogEntry(tmpBuf);
			}

			goto exit;
		}

		/* Drop from the directory flat files */
		if ((dir = AllocateDir(fileName)) == NULL)
		{
			{
				char		tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];

				snprintf(tmpBuf, sizeof(tmpBuf), "error reading directory identifier '%s' errno '%d' ",
						 fileName,
						 errno);

				FileRep_InsertConfigLogEntry(tmpBuf);
			}

			goto exit;
		}

		while ((de = ReadDir(dir, fileName)) != NULL)
		{
			if (strcmp(de->d_name, ".") == 0 ||
				strcmp(de->d_name, "..") == 0)
				continue;

			sprintf(path, "%s/%s", fileName, de->d_name);

			errno = 0;
			if (unlink(path))
			{
				if (errno == EPERM)
				{
					if ((stat(path, &st) >= 0) && (S_ISDIR(st.st_mode)))
						continue;
				}

				{
					char		tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];

					snprintf(tmpBuf, sizeof(tmpBuf), "unlink failed identifier '%s' errno '%d' ",
							 fileName,
							 errno);

					FileRep_InsertConfigLogEntry(tmpBuf);
				}
			}
		}
	}

exit:
	if (dir)
	{
		FreeDir(dir);
	}
	return;
}

/*
 *
 */
static int
SegmentFileFillZero(File fd, int64 writeOffset)
{
	int			status = STATUS_OK;
	int64		startOffset = 0;
	int64		seekResult;

	char		zbuffer[BLCKSZ];

	Assert(writeOffset < RELSEG_SIZE * BLCKSZ);

	MemSet(zbuffer, 0, sizeof(zbuffer));

	errno = 0;
	startOffset = FileSeek(fd, 0L, SEEK_END);

	if (startOffset < 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("mirror failure, "
						"could not zero segment file, "
						"could not seek to end of file fd '%d' : %m, "
						"failover requested",
						fd),
				 errhint("run gprecoverseg to re-establish mirror connectivity"),
				 FileRep_errcontext()));

		status = STATUS_ERROR;
		return status;
	}

	if (startOffset >= writeOffset)
	{
		return status;
	}

	Assert(startOffset < writeOffset);

	errno = 0;
	seekResult = FileSeek(fd, startOffset, SEEK_SET);
	if (seekResult != startOffset)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("mirror failure, "
						"could not zero segment file, "
						"could not seek to position " INT64_FORMAT " of file fd '%d' : %m, "
						"failover requested",
						startOffset,
						fd),
				 errhint("run gprecoverseg to re-establish mirror connectivity"),
				 FileRep_errcontext()));

		status = STATUS_ERROR;
		return status;
	}

	while (startOffset < writeOffset)
	{
		errno = 0;

		if ((int) FileWrite(fd, zbuffer, BLCKSZ) != (int) BLCKSZ)
		{
			int			save_errno = errno;

			/*
			 * If we fail to make the file, delete it to release disk space
			 */

			/* if write didn't set errno, assume problem is no disk space */
			errno = save_errno ? save_errno : ENOSPC;

			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("mirror failure, "
							"could not zero segment file, "
							"could not write to file fd '%d' : %m, "
							"failover requested",
							fd),
					 errhint("run gprecoverseg to re-establish mirror connectivity"),
					 FileRep_errcontext()));

			status = STATUS_ERROR;
			return status;
		}
		startOffset += BLCKSZ;
	}

	errno = 0;
	if (FileSync(fd) != 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("mirror failure, "
						"could not zero segment file, "
						"could not fsync file fd '%d' : %m, "
						"failover requested",
						fd),
				 errhint("run gprecoverseg to re-establish mirror connectivity"),
				 FileRep_errcontext()));

		status = STATUS_ERROR;
	}

	return status;
}

/*
 *
 */
static int
FileRepMirror_Drop(FileName fileName)
{
	File		fd = 0;
	int			fileMode = 0600;
	int			status = STATUS_OK;

	fd = FileRepMirror_GetFileDescriptor(fileName);

	if (fd < 0)
	{
		errno = 0;
		fd = PathNameOpenFile(fileName,
							  O_WRONLY,
							  fileMode);

		if (fd < 0 && errno == ENOENT)
		{
			fd = 0;
			return status;
		}

		if (fd < 0)
		{
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("mirror failure, "
							"could not open file '%s' to issue drop : %m, "
							"failover requested",
							fileName),
					 errhint("run gprecoverseg to re-establish mirror connectivity"),
					 FileRep_errcontext()));

			status = STATUS_ERROR;

			return status;
		}
	}
	else
	{
		FileRepMirror_RemoveFileName(fileName);
	}

	SetDeleteOnExit(fd);
	FileClose(fd);

	return status;
}

/*
 *
 */
static int
FileRepMirror_DropBufferPoolSegmentFiles(FileName fileName, int segmentFileNum)
{
	char		tmp[MAXPGPATH + 1];
	char		fileNameLocal[MAXPGPATH + 1];
	int			status = STATUS_OK;
	int			segmentFileNumLocal;

	/*
	 * We do this in two passes because it is safer to drop the files in
	 * reverse order so as to prevent the creation of holes, but we need to
	 * scan forward to know what files actually exist.
	 */
	for (segmentFileNumLocal = 1; /* break in code */ ; segmentFileNumLocal++)
	{
		struct stat sbuf;

		snprintf(tmp, sizeof(tmp), "%s.%d", fileName, segmentFileNumLocal);
		if (stat(tmp, &sbuf) < 0)
			break;				/* No such file, loop is done */
	}

	/* second pass perform the drops in reverse order: important for REDO */
	for (segmentFileNumLocal--; segmentFileNumLocal > segmentFileNum; segmentFileNumLocal--)
	{
		sprintf(fileNameLocal, "%s.%u", fileName, segmentFileNumLocal);
		status = FileRepMirror_Drop(fileNameLocal);
		if (status != STATUS_OK)
		{
			break;
		}
	}

	return status;
}
