/*-------------------------------------------------------------------------
 *
 * cdbfilerepmirrorack.c
 *
 * Portions Copyright (c) 2009-2010 Greenplum Inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbfilerepmirrorack.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <signal.h>
#include <sys/time.h>

#include "cdb/cdbvars.h"
#include "cdb/cdbfilerepservice.h"
#include "cdb/cdbfilerepmirrorack.h"
#include "cdb/cdbfilerepconnclient.h"
#include "storage/lwlock.h"
#include "utils/faultinjector.h"


/****************************************************************
 * FILE REP ACK SENDER Process definitions
 ****************************************************************/

static int	FileRepAckMirror_RunSender(void);
static int FileRepAckMirror_ConstructAndInsertMessage(
										   FileRepIdentifier_u fileRepIdentifier,
										   FileRepRelationType_e fileRepRelationType,
										   FileRepOperation_e fileRepOperation,
										   FileRepOperationDescription_u fileRepOperationDescription,
										   FileRepAckState_e fileRepAckState,
										   uint32 messageBodyLength,
										   char *messageBody);

/****************************************************************
 * Routines called by CONSUMER PROCESSES
 ****************************************************************/

/*
 * FileRepAckMirror_ConstructAndInsertMessage
 *			a) reserve ack shared memory
 *			b) construct message header
 *			c) calculate message body crc
 *			d) calculate message header crc
 *			e) insert message header, message header crc and message body into ack shared memory
 *			f) set filerep message state to 'ready' so that message can be consumed
 *			g) signal send ack process so message will be sent to primary immediately
 */
static int
FileRepAckMirror_ConstructAndInsertMessage(
										   FileRepIdentifier_u fileRepIdentifier,
										   FileRepRelationType_e fileRepRelationType,
										   FileRepOperation_e fileRepOperation,
										   FileRepOperationDescription_u fileRepOperationDescription,
										   FileRepAckState_e fileRepAckState,
										   uint32 messageBodyLength,
										   char *messageBody)
{

	FileRepShmemMessageDescr_s *fileRepShmemMessageDescr = NULL;
	FileRepMessageHeader_s *fileRepMessageHeader = NULL;
	pg_crc32   *fileRepMessageHeaderCrc = 0;
	pg_crc32	fileRepMessageHeaderCrcLocal = 0;
	char	   *msgPositionInsert = NULL;
	char	   *fileRepMessageBody = NULL;
	uint32		msgLength = 0;
	uint32		spareField = 0;

	Assert(messageBodyLength == 0);

	msgLength = sizeof(FileRepMessageHeader_s) + sizeof(pg_crc32) + messageBodyLength;

	/*
	 * Shared memory is reserved in advance in order to avoid holding lock
	 * during memcpy() and Crc calculation...
	 *
	 * Construct FileRepShmemMessageDescr_s
	 */
	msgPositionInsert = FileRep_ReserveShmem(
											 fileRepAckShmemArray[FILEREP_OUTGOING_MESSAGE_QUEUE],
											 msgLength,
											 &spareField,
											 fileRepOperation,
											 FileRepAckShmemLock);

	if (msgPositionInsert == NULL)
	{

		ereport(WARNING,
				(errmsg("mirror failure, "
						"could not queue ack message to be sent to primary, "
						"failover requested"),
				 errhint("run gprecoverseg to re-establish mirror connectivity"),
				 FileRep_errdetail(fileRepIdentifier,
								   fileRepRelationType,
								   fileRepOperation,
								   FILEREP_UNDEFINED),
				 FileRep_errdetail_ShmemAck(),
				 FileRep_errcontext()));

		return STATUS_ERROR;
	}

	fileRepMessageHeader =
		(FileRepMessageHeader_s *) (msgPositionInsert +
									sizeof(FileRepShmemMessageDescr_s));

	fileRepMessageHeader->fileRepMessageHeaderVersion = FileRepMessageHeaderVersionOne;

	fileRepMessageHeader->fileRepRelationType = fileRepRelationType;

	fileRepMessageHeader->fileRepOperation = fileRepOperation;

	fileRepMessageHeader->fileRepIdentifier = fileRepIdentifier;

	fileRepMessageHeader->fileRepAckState = fileRepAckState;

	fileRepMessageHeader->messageCount = spareField;

	fileRepMessageHeader->messageBodyLength = messageBodyLength;

	if (messageBodyLength)
	{
		FileRep_CalculateCrc((char *) messageBody,
							 messageBodyLength,
							 &fileRepMessageHeader->fileRepMessageBodyCrc);
	}
	else
	{
		fileRepMessageHeader->fileRepMessageBodyCrc = 0;
	}

	switch (fileRepOperation)
	{
		case FileRepOperationReconcileXLogEof:
		case FileRepOperationValidation:
		case FileRepOperationCreate:
		case FileRepOperationStartSlruChecksum:
		case FileRepOperationVerifySlruDirectoryChecksum:
			fileRepMessageHeader->fileRepOperationDescription = fileRepOperationDescription;
			break;

		default:
			break;
	}

	/*
	 * The following fields are not in use during fsync(). They were zeroed
	 * during reserving message buffer.  a) fileRepOperationDescription b)
	 * fileRepMessageBodyCrc c) messageCount
	 */

	fileRepMessageHeaderCrc =
		(pg_crc32 *) (msgPositionInsert +
					  sizeof(FileRepMessageHeader_s) +
					  sizeof(FileRepShmemMessageDescr_s));

	FileRep_CalculateCrc((char *) fileRepMessageHeader,
						 sizeof(FileRepMessageHeader_s),
						 &fileRepMessageHeaderCrcLocal);

	*fileRepMessageHeaderCrc = fileRepMessageHeaderCrcLocal;

	FileRep_InsertLogEntry(
						   "M_ConstructAndInsertMessageAck",
						   fileRepIdentifier,
						   fileRepRelationType,
						   fileRepOperation,
						   fileRepMessageHeaderCrcLocal,
						   FILEREP_UNDEFINED,
						   fileRepAckState,
						   FILEREP_UNDEFINED,
						   fileRepMessageHeader->messageCount);

	if (messageBodyLength)
	{
		fileRepMessageBody = (char *) (msgPositionInsert +
									   sizeof(FileRepMessageHeader_s) +
									   sizeof(FileRepShmemMessageDescr_s) +
									   sizeof(pg_crc32));

		memcpy(fileRepMessageBody, messageBody, messageBodyLength);
	}

	fileRepShmemMessageDescr =
		(FileRepShmemMessageDescr_s *) msgPositionInsert;

	fileRepShmemMessageDescr->messageSync =
		FileRep_IsOperationSynchronous(fileRepOperation);

	fileRepShmemMessageDescr->messageState = FileRepShmemMessageStateReady;

	LWLockAcquire(FileRepAckShmemLock, LW_EXCLUSIVE);

	FileRep_IpcSignal(fileRepIpcArray[fileRepAckShmemArray[FILEREP_OUTGOING_MESSAGE_QUEUE]->ipcArrayIndex]->semC,
					  &fileRepIpcArray[fileRepAckShmemArray[FILEREP_OUTGOING_MESSAGE_QUEUE]->ipcArrayIndex]->refCountSemC);
	LWLockRelease(FileRepAckShmemLock);

	return STATUS_OK;
}

int
FileRepAckMirror_Ack(
					 FileRepIdentifier_u fileRepIdentifier,
					 FileRepRelationType_e fileRepRelationType,
					 FileRepOperation_e fileRepOperation,
					 FileRepOperationDescription_u fileRepOperationDescription,
					 FileRepAckState_e fileRepAckState,
					 uint32 messageBodyLength,
					 char *messageBody)
{
	int			status = STATUS_OK;

	status = FileRepAckMirror_ConstructAndInsertMessage(
														fileRepIdentifier,
														fileRepRelationType,
														fileRepOperation,
														fileRepOperationDescription,
														fileRepAckState,
														messageBodyLength,
														messageBody);

	return status;
}

/****************************************************************
 * FILEREP SUB-PROCESS (FileRep Ack Mirror SENDER Process)
 ****************************************************************/
/*
 *
 * FileRepPrimary_StartSender
 */
void
FileRepAckMirror_StartSender(void)
{
	int			status = STATUS_OK;
	int			retry = 0;
	struct timeval currentTime;
	pg_time_t	beginTime = 0;
	pg_time_t	endTime = 0;

	FileRep_InsertConfigLogEntry("start sender ack");

	while (1)
	{

		if (status != STATUS_OK)
		{
			FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
			FileRepSubProcess_SetState(FileRepStateFault);
		}

		while (FileRepSubProcess_GetState() == FileRepStateInitialization ||
			   FileRepSubProcess_GetState() == FileRepStateFault ||

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

		status = FileRepConnClient_EstablishConnection(
													   fileRepPrimaryHostAddress,
													   fileRepPrimaryPort,
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
														   fileRepPrimaryHostAddress,
														   fileRepPrimaryPort,
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

		status = FileRepAckMirror_RunSender();

	} //while (1)

		FileRepConnClient_CloseConnection();

	return;
}

/*
 * SenderLoop
 *
 */
static int
FileRepAckMirror_RunSender(void)
{
	FileRepShmemMessageDescr_s *fileRepShmemMessageDescr = NULL;
	char	   *fileRepMessage;
	int			status = STATUS_OK;
	bool		movePositionConsume = FALSE;
	FileRepConsumerProcIndex_e messageType;
	FileRepMessageHeader_s *fileRepMessageHeader;
	FileRepShmem_s *fileRepAckShmem = NULL;

	FileRep_InsertConfigLogEntry("run sender ack");

	fileRepAckShmem = fileRepAckShmemArray[FILEREP_OUTGOING_MESSAGE_QUEUE];

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
			if (FileRepSubProcess_GetState() != FileRepStateReady)
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
		} //while internal
			fileRepAckShmem->consumeCount++;

		LWLockRelease(FileRepAckShmemLock);

		FileRepSubProcess_ProcessSignals();
		if (FileRepSubProcess_GetState() != FileRepStateReady)
		{
			break;
		}

		FileRep_InsertLogEntry(
							   "M_RunSenderAck",
							   FileRep_GetFlatFileIdentifier("", ""),
							   FileRepRelationTypeNotSpecified,
							   FileRepOperationNotSpecified,
							   FILEREP_UNDEFINED,
							   FILEREP_UNDEFINED,
							   FileRepAckStateNotInitialized,
							   FILEREP_UNDEFINED,
							   FILEREP_UNDEFINED);

		SIMPLE_FAULT_INJECTOR(FileRepSender);

		fileRepMessage = (char *) (fileRepAckShmem->positionConsume +
								   sizeof(FileRepShmemMessageDescr_s));

		fileRepMessageHeader = (FileRepMessageHeader_s *) (fileRepAckShmem->positionConsume +
														   sizeof(FileRepShmemMessageDescr_s));

		messageType = FileRepMessageTypeXLog;

		if (!FileRepConnClient_SendMessage(
										   messageType,
										   fileRepShmemMessageDescr->messageSync,
										   fileRepMessage,
										   fileRepShmemMessageDescr->messageLength))
		{

			ereport(WARNING,
					(errcode_for_socket_access(),
					 errmsg("mirror failure, "
							"could not sent ack message to primary : %m, "
							"failover requested"),
					 errhint("run gprecoverseg to re-establish mirror connectivity"),
					 FileRep_errdetail_ShmemAck(),
					 FileRep_errcontext()));

			status = STATUS_ERROR;
			break;
		}

		movePositionConsume = TRUE;
	} //while (1)

		FileRepConnClient_CloseConnection();

	return status;
}
