/*-------------------------------------------------------------------------
 *
 * cdbfilerep.c
 *
 * Portions Copyright (c) 2009-2010 Greenplum Inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbfilerep.c
 *
 *-------------------------------------------------------------------------
 */

/*
 * FileRep process (main thread) forked by postmaster.
 *
 * Responsibilities of this module.
 *		*)
 *
 */
#include "postgres.h"

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "miscadmin.h"
#include "catalog/catalog.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbfilerep.h"
#include "cdb/cdbfilerepprimary.h"
#include "cdb/cdbfilerepprimaryack.h"
#include "cdb/cdbfilerepmirror.h"
#include "cdb/cdbfilerepmirrorack.h"
#include "cdb/cdbfilerepconnserver.h"
#include "cdb/cdbfilerepservice.h"
#include "cdb/cdbpersistenttablespace.h"
#include "cdb/cdbresynchronizechangetracking.h"
#include "libpq/pqsignal.h"
#include "libpq/ip.h"
#include "postmaster/fork_process.h"
#include "storage/fd.h"
#include "postmaster/primary_mirror_mode.h"
#include "storage/ipc.h"
#include "storage/pmsignal.h"
#include "storage/proc.h"
#include "storage/shmem.h"
#include "tcop/tcopprot.h"
#include "utils/guc.h"
#include "utils/ps_status.h"
#include "utils/memutils.h"
#include "utils/faultinjector.h"

/****************************************************************
 * FILEREP PROCESS definitions
 ****************************************************************/
/*
 * GUC parameter
 *			file_rep_message_body_length
 *
 * Size (in bytes) for FileRep Message Body (contains user data).
 * Default is 32 kB.
 * NOTE add to guc.c "extern uint32 file_rep_message_body_length"
 */
int file_rep_message_body_length = 32 * 1024;

static FileRepState_e fileRepState = FileRepStateNotInitialized;
	/* state of FileRep process */
FileRepRole_e	fileRepRole = FileRepRoleNotInitialized;
SegmentState_e	segmentState = SegmentStateNotInitialized;
DataState_e		dataState = DataStateNotInitialized;

char *fileRepPrimaryHostAddress = NULL;
int fileRepPrimaryPort = -1;
char *fileRepMirrorHostAddress = NULL;
int fileRepMirrorPort = -1;
bool fileRepFullResync = false;

/****************************************/
FileRepConsumerProcIndex_e 	fileRepProcIndex = FileRepMessageTypeXLog;

static	MemoryContext	fileRepMemoryContext;
/*
 * Parameters set by signal handlers for later service n the main loop
 */
static volatile sig_atomic_t reloadConfigFile = false;

static volatile sig_atomic_t shutdownRequested = false;
	/* graceful shutdown performed by SIGUSR2 signal from postmaster */

static volatile sig_atomic_t immediateShutdownRequested = false;
	/* graceful shutdown performed by SIGQUIT signal from postmaster */

/** true if, upon exit, we should error with 'immediateShutdown' exit code */
static volatile sig_atomic_t exitWithImmediateShutdown = false;

static volatile sig_atomic_t childTermination = false;
	/* child termination of the main file rep process */

/* state change informed by SIGUSR1 signal from postmaster...when state change request comes in
 *  this counter is incremented.
 */
static volatile sig_atomic_t stateChangeRequestCounter = 0;

/**
 * This value increases when we process state change requests.  There is no pending state change request if
 *   lastChangeRequestProcessCounterValue == stateChangeRequestCounter
 *
 * Note that this value is not actually updated in the signal handlers, but must match or exceed the size of
 *    stateChangeRequestCounter so we use its type
 */
static volatile sig_atomic_t lastChangeRequestProcessCounterValue = 0;

static void FileRep_SigHupHandler(SIGNAL_ARGS);
static void FileRep_ImmediateShutdownHandler(SIGNAL_ARGS);
static void FileRep_ShutdownHandler(SIGNAL_ARGS);
static void FileRep_ChildTerminationHandler(SIGNAL_ARGS);
static void FileRep_StateHandler(SIGNAL_ARGS);
static void FileRep_HandleCrash(SIGNAL_ARGS);

static bool FileRep_IsAnyChildAlive(void);
static bool FileRep_IsAnyBackendChildAlive(void);
static bool FileRep_ProcessSignals(void);
static void FileRep_ConfigureSignals(void);
static void FileRep_IpcSignalAllInternal(int ii);

static pg_crc32 FileRep_CalculateAdler32(unsigned char *buf, int len);

#define MaxFileRepSubProc FileRepProcessType__EnumerationCount

#define EXIT_STATUS_0(st)  ((st) == 0)
#define EXIT_STATUS_1(st)  (WIFEXITED(st) && WEXITSTATUS(st) == 1)
#define EXIT_STATUS_2(st)  (WIFEXITED(st) && WEXITSTATUS(st) == 2)

/**
 * the level of logging for filerep process messages (having to do with process create/destroy)
 * It can be used only for testing purpose (set to LOG). Logging in signal handlers is not allowed, can cause deadlock.
 * Remove the logs when code gets more stable.
 */
#define FILEREP_PROCESS_LOG_LEVEL DEBUG5

typedef struct FileRepSubProc {

	pid_t					pid;
	FileRepProcessType_e	fileRepProcessType;

} FileRepSubProc;

/*
 * FileRepSubProcList is in shared memory and keeps track of
 * filerep subprocesses.  Normally their lifecycle is handled
 * by filerep main process, hence this table is touched by only
 * filerep main process.  However, in case the main process
 * dies unexpectedly, postmaster will send immediate shutdown
 * on behalf of the main process.  Nobody except those two
 * should touch this table.  Also for this reason, we don't
 * synchronize the access to this table.
 */
static FileRepSubProc FileRepSubProcListInitial[MaxFileRepSubProc] =
{
	{0, FileRepProcessTypeNotInitialized},
	{0, FileRepProcessTypeMain},
	{0, FileRepProcessTypePrimarySender},
	{0, FileRepProcessTypeMirrorReceiver},
	{0, FileRepProcessTypeMirrorConsumer},
	{0, FileRepProcessTypeMirrorSenderAck},
	{0, FileRepProcessTypePrimaryReceiverAck},
	{0, FileRepProcessTypePrimaryConsumerAck},
	{0, FileRepProcessTypePrimaryRecovery},
	{0, FileRepProcessTypeMirrorConsumerWriter},
	{0, FileRepProcessTypeMirrorConsumerAppendOnly1},
	{0, FileRepProcessTypeResyncManager},
	{0, FileRepProcessTypeResyncWorker1},
	{0, FileRepProcessTypeResyncWorker2},
	{0, FileRepProcessTypeResyncWorker3},
	{0, FileRepProcessTypeResyncWorker4},
}, *FileRepSubProcList;


/*
 * gettext() can't be used in a static initializer... This breaks nls builds.
 * So, to work around this issue, I've made _() be a no-op.
 */
#undef _
#define _(x) x

const char*
FileRepProcessTypeToString[] = {
	_("filerep uninitialized process"),
	_("filerep main process"), /* note that this one is actually changed when we set the title */
	_("primary sender process"),
	_("mirror receiver process"),
	_("mirror consumer process"),

	_("mirror sender ack process"),
	_("primary receiver ack process"),
	_("primary consumer ack process"),
	_("primary recovery process"),
	_("mirror consumer writer process"),

	_("mirror consumer append only process"),

	_("primary resync manager process"),

	_("primary resync worker #1 process"),
	_("primary resync worker #2 process"),
	_("primary resync worker #3 process"),
	_("primary resync worker #4 process"),

};

const char*
FileRepRoleToString[] = {
	_("role not initialized"),
	_("role not configured"),
	_("primary role"),
	_("mirror role"),
};

const char*
SegmentStateToString[] = {
	_("not initialized"),
	_("initialization and recovery"),
	_("transition to change tracking"),
	_("transition to resync"),
	_("transition to sync"),
	_("up and running"),
	_("change tracking disabled"),
	_("in fault"),
	_("in backends shutdown"),
	_("in shutdown"),
	_("in immediate shutdown"),
};

const char*
DataStateToString[] = {
	_("not initialized"),
	_("sync"),
	_("change tracking"),
	_("resync"),
};

const char*
FileRepStateToString[] = {
	_("not initialized"),
	_("initialization and recovery"),
	_("up and running"),
	_("fault"),
	_("in backends shutdown"),
	_("in shutdown"),
};

const char*
FileRepOperationToString[] = {
	_("open"),
	_("write"),
	_("flush (fsync)"),
	_("close"),
	_("flush (fsync) and close"),
	_("truncate"),
	_("create"),
	_("drop"),
	_("reconcile xlog"),
	_("link or rename"),
	_("shutdown"),
	_("in sync transition"),
	_("heart beat"),
	_("create and open"),
	_("validation filespace dir or existence"),
	_("drop files from dir"),
	_("drop temporary files"),
	_("start checksum computation of a SLRU directory"),
	_("verify checksum of a SLRU directory"),
	_("not specified"),
};

const char*
FileRepRelationTypeToString[] = {
	_("flat file"),
	_("append only"),
	_("buffer pool"),
	_("bulk"),
	_("directory"),
	_("control message"),
	_("not specified"),
};

const char *
FileRepAckStateToString[] = {
	_("not initialized"),
	_("waiting for ack"),
	_("ack completed"),
	_("mirror in fault"),
};

const char*
FileRepConsumerProcIndexToString[] = {
	_("xlog"),
	_("append only"),
	_("writer"),
	_("shutdown"),
	_("undefined"),
};

const char*
FileRepStatusToString[] = {
	_("success"),
	_("not a directory"),
	_("no such file or directory"),
	_("error reading directory"),
	_("directory not empty"),
	_("file exists"),
	_("no space"),
	_("permission denied"),
	_("read-only file system"),
	_("mirror loss occurred"),
	_("mirror error"),
	_("slru checksum failed")
};

const char *
FileRepOperationVerificationTypeToString[]= {
		_("not initialized"),
		_("summary items"),
		_("enhanced block hash"),
		_("send request parameters"),
		_("directory contents"),

};
/****************************************************************
 * FILEREP_SHARED MEMORY
 ****************************************************************/
/*
 * FileRep Shared Memory for communication of messages to be mirrored
 *		a) between backends and FileRep process on a primary
 *		b) between Receiver and Consumer threads on a mirror
 *
 *	FileRep Shared Memory is initialized by Postmaster process
 *	before FileRep Process is
 *
 * Messages are inserted into shared memory in wraparound mode (from
 * beginPosition to wraparoundPosition and then back to beginPosition).
 * Messages are consumed in order (first inserted first consumed).
 *
 * Messages (FileRepMessage_s) that are mirrored are variable sizes.
 * Message size depends on operation type.
 *		a) Write operation sends data in addition to metadata.
 *		b) Open, Flush, Close operation send metadata only.
 *
 * Variable messages size means that
 *		a) Messages are inserted to different position in shared memory
 *		b) Messages are not alligned with endPosition
 *
 * beginPosition
 *	|	 -------------------------------------------------
 *	|	| FileRepShmem_s								 |
 * 	 =>  -------------------------------------------------
 *		| FileRepShmemMessageDescr_s | FileRepMessage_s  |
 *		 -------------------------------------------------
 *		| FileRepShmemMessageDescr_s | FileRepMessage_s  |
 *		 -------------------------------------------------
 *		|												 |
 *		 -------------------------------------------------
 *		|												 |
 *		 -------------------------------------------------
 *		| FileRepShmemMessageDescr_s | FileRepMessage_s  |
 *		 -------------------------------------------------  <= endPosition
 *													  ^
 *													  |
 *											wraparoundPosition
 */

/*
 * GUC parameter
 *			file_rep_mirror_consumer_process_count
 *
 * Number of processes to consume (perform file operations) messages on mirror.
 */
int		file_rep_mirror_consumer_process_count = 4;

/*
 * GUC parameter
 *			file_rep_buffer_size
 *
 * Size (in bytes) for FileRep Shared Memory.
 * Default=2MB MIN=1MB MAX=16MB
 */
int		file_rep_buffer_size = 2 * 1024 * 1024;

/*
 * GUC parameter
 *			file_rep_ack_buffer_size
 *
 * Size (in bytes) for FileRepAck Shared Memory.
 * Default=512kB MIN=128kB MAX=1MB
 */
int		file_rep_ack_buffer_size = 512 * 1024;

/*
 * GUC parameter
 *
 * If a mirror does not respond in gp_segment_connect_timeout time (in seconds),
 * it is deemed inactive or down.
 *
 * Number of retries to insert message into shared memory. Buffer
 * may be full due to i.e. network down, mirror host down,
 * slow processing, ...
 */
int		file_rep_retry = 2000;  // 186 seconds

/**
 * GUC parameter
 *          file_rep_min_data_before_flush
 *
 * Once a client's buffer has >= this amount of data (in kb) then the client will flush
 */
int file_rep_min_data_before_flush = 128 * 1024;


/*
 * GUC parameter
 *     	    file_rep_socket_timeout
 *
 * Timeout (in seconds) if we cannot write to socket (the socket is blocking)
 */
int file_rep_socket_timeout = 10;

FileRepShmem_s	*fileRepShmemArray[FILEREP_SHMEM_MAX_SLOTS];
FileRepShmem_s	*fileRepAckShmemArray[FILEREP_ACKSHMEM_MAX_SLOTS];

static void
FileRep_ShmemInitArray(FileRepShmem_s	**array, int arrayCount, int bufferSize, bool initIpcArrayIndex);

static void FileRepIpc_ShmemReInit(void);

FileRepIpcShmem_s	*fileRepIpcArray[FILEREP_MAX_IPC_ARRAY];

int
FileRepSemas(void)
{
	/*---------------------------------------------------------------------
	 *	PRIMARY
	 *		1) backend and sender (FileRepShmem_s = 2)
	 *		2) Receiver Ack and Consumer Ack (FileRepAckShmem_s = 2)
	 *		3) Consumer Ack and backend (IsOperationCompleted = 1)
	 *		4) backend and backend (InsertNewHashEntry = 1)
     *      5) receiver ack and verification
	 *
	 *	MIRROR
	 *		1) receiver and consumer XLOG (FileRepShmem_s = 2)
	 *		2) receiver and consumer bgwriter (FileRepShmem_s =2)
	 *		3) receiver and consumer Append Only 1 (FileRepShmem_s = 2)
	 *		4) receiver and consumer Append Only 2 (FileRepShmem_s = 2)
	 *		5) consumers and sender ack (FileRepAckShmem_s = 2)
	 *---------------------------------------------------------------------
	 */
	/*---------------------------------------------------------------------
	 *	FileRep_IpcSignal() and reference count are protected by the following locks
	 *
	 *  PRIMARY
	 *		1) LWLockAcquire(FileRepShmemLock, LW_EXCLUSIVE);
	 *		2) LWLockAcquire(FileRepAckShmemLock, LW_EXCLUSIVE);
	 *		3) LWLockAcquire(FileRepAckHashShmemLock, LW_EXCLUSIVE);
	 *      4) LWLockAcquire(FileRepAckShmemLock, LW_EXCLUSIVE);
	 *
	 *	MIRROR
	 *		1) LWLockAcquire(FileRepShmemLock, LW_EXCLUSIVE);
	 *		2) LWLockAcquire(FileRepShmemLock, LW_EXCLUSIVE);
	 *		3) LWLockAcquire(FileRepShmemLock, LW_EXCLUSIVE);
	 *		4) LWLockAcquire(FileRepShmemLock, LW_EXCLUSIVE);
	 *		5) LWLockAcquire(FileRepAckShmemLock, LW_EXCLUSIVE);
	 *---------------------------------------------------------------------
	 */

	return(FILEREP_MAX_SEMAPHORES);
}

Size
FileRepIpc_ShmemSize(void)
{
	Size	size;

	size = sizeof(FileRepIpcShmem_s);

	size = mul_size(size, FILEREP_MAX_IPC_ARRAY);

	return size;
}

void
FileRepIpc_ShmemInit(void)
{
	int		ii;
	bool	foundPtr;
	char label[16];

	for (ii=0; ii < FILEREP_MAX_IPC_ARRAY; ii++)
	{
		sprintf(label, "filerep ipc %d", ii);
		fileRepIpcArray[ii] = (FileRepIpcShmem_s *) ShmemInitStruct(label,
																	sizeof(FileRepIpcShmem_s),
																	&foundPtr);

		if (fileRepIpcArray[ii] == NULL) {
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 (errmsg("not enough shared memory for mirroring"))));
		}

		if (! foundPtr) {
			MemSet(fileRepIpcArray[ii], 0, sizeof(FileRepIpcShmem_s));
		}

		PGSemaphoreCreate(&fileRepIpcArray[ii]->semP);
		PGSemaphoreCreate(&fileRepIpcArray[ii]->semC);

		fileRepIpcArray[ii]->refCountSemP = 0;
		fileRepIpcArray[ii]->refCountSemC = 0;
	}
}

static void
FileRepIpc_ShmemReInit(void)
{
	int		ii,jj;

	for (ii=0; ii < FILEREP_MAX_IPC_ARRAY; ii++)
	{
		if (fileRepIpcArray[ii] == NULL)
		{
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 (errmsg("shared memory for filerep ipc not initialized"))));
		}

		for (jj=0; jj < fileRepIpcArray[ii]->refCountSemP; jj++)
		{
			PGSemaphoreUnlock(&fileRepIpcArray[ii]->semP);
		}

		for (jj=0; jj < fileRepIpcArray[ii]->refCountSemC; jj++)
		{
			PGSemaphoreUnlock(&fileRepIpcArray[ii]->semC);
		}

		PGSemaphoreReset(&fileRepIpcArray[ii]->semP);
		PGSemaphoreReset(&fileRepIpcArray[ii]->semC);

		fileRepIpcArray[ii]->refCountSemP = 0;
		fileRepIpcArray[ii]->refCountSemC = 0;
	}
}

void
FileRep_IpcWait(PGSemaphoreData sem, int *refCount, LWLockId lockId)
{
	bool success = PGSemaphoreLockInterruptable(&sem);
	if (!success)
	{
		LWLockAcquire(lockId, LW_EXCLUSIVE);
		(*refCount)--;
		LWLockRelease(lockId);
	}
}

void
FileRep_IpcSignal(PGSemaphoreData sem, int *refCount)
{
	int ii;

	for (ii=0; ii < *refCount; ii++)
	{
		PGSemaphoreUnlock(&sem);
	}

	*refCount = 0;
}

static void
FileRep_IpcSignalAllInternal(int ii)
{
	int jj;

	for (jj=0; jj < fileRepIpcArray[ii]->refCountSemP; jj++)
	{
		PGSemaphoreUnlock(&fileRepIpcArray[ii]->semP);
	}

	fileRepIpcArray[ii]->refCountSemP = 0;

	for (jj=0; jj < fileRepIpcArray[ii]->refCountSemC; jj++)
	{
		PGSemaphoreUnlock(&fileRepIpcArray[ii]->semC);
	}

	fileRepIpcArray[ii]->refCountSemC = 0;
}

void
FileRep_IpcSignalAll(void)
{
	int ii;

	for (ii=0; ii < FILEREP_MAX_IPC_ARRAY; ii++)
	{
		switch (fileRepRole)
		{
			case FileRepPrimaryRole:
				switch (ii)
				{
					case 0:
						LWLockAcquire(FileRepShmemLock, LW_EXCLUSIVE);
						FileRep_IpcSignalAllInternal(ii);
						LWLockRelease(FileRepShmemLock);
						break;
					case 1:
						LWLockAcquire(FileRepAckShmemLock, LW_EXCLUSIVE);
						FileRep_IpcSignalAllInternal(ii);
						LWLockRelease(FileRepAckShmemLock);
						break;
					case 3:
					case 4:
						LWLockAcquire(FileRepAckHashShmemLock, LW_EXCLUSIVE);
						FileRep_IpcSignalAllInternal(ii);
						LWLockRelease(FileRepAckHashShmemLock);
						break;
					default:
						break;
				}
				break;

			case FileRepMirrorRole:
				switch (ii)
				{
					case 0:
					case 1:
					case 2:
					case 3:
						LWLockAcquire(FileRepShmemLock, LW_EXCLUSIVE);
						FileRep_IpcSignalAllInternal(ii);
						LWLockRelease(FileRepShmemLock);
						break;
					case 4:
						LWLockAcquire(FileRepAckShmemLock, LW_EXCLUSIVE);
						FileRep_IpcSignalAllInternal(ii);
						LWLockRelease(FileRepAckShmemLock);
						break;
					default:
						break;
				}
				break;

			default:
				break;
		}
	}
}

/*
 * FileRep_SubProcShmemSize()
 *		Calculate size required for subprocess table.
 *
 */
Size
FileRep_SubProcShmemSize(void)
{
	Size	size;

	size = sizeof(FileRepSubProcListInitial);

	return MAXALIGN(size);
}

/*
 * FileRep_SubProcShmemInit()
 *			Allocate and initialize FileRep subprocess Shared Memory
 *
 */
void
FileRep_SubProcShmemInit(void)
{
	bool		found;

	FileRepSubProcList = (FileRepSubProc *)
							ShmemInitStruct("filerep subprocess list",
									sizeof(FileRepSubProcListInitial),
									&found);

	if (!found)
		memcpy(FileRepSubProcList, FileRepSubProcListInitial,
				sizeof(FileRepSubProcListInitial));
}

static FileRepShmem_s *FileRep_ShmemInitOneBuffer(const char *bufferLabel, int shmemSize, int bufSize)
{
	bool	foundPtr;
	FileRepShmem_s *result;

	Assert( shmemSize >= bufSize + sizeof(FileRepShmem_s));

	result = (FileRepShmem_s *)
			ShmemInitStruct(bufferLabel, shmemSize, &foundPtr);

	if (result == NULL) {
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				(errmsg("not enough shared memory for mirroring"))));
	}

	if (! foundPtr) {
		MemSet(result, 0, sizeof(FileRepShmem_s));
	}

	result->positionBegin =
		((char *) result) + sizeof(FileRepShmem_s);

	MemSet(result->positionBegin, 0, bufSize);

	result->positionEnd =
		result->positionBegin + bufSize;

	result->positionInsert = result->positionBegin;

	result->positionConsume = result->positionBegin;

	result->positionWraparound = result->positionEnd;

	result->insertCount = 0;

	result->consumeCount = 0;

	result->state = FileRepStateNotInitialized;

	/*
	 * positionWraparound is initialized
	 * when message is inserted at the positionBegin.
	 */

	return result;
}

/*
 * FileRep_ShmemInit()
 *			Allocate and initialize FileRep Shared Memory
 *
 */
void
FileRep_ShmemInit(void)
{
	fileRepShmemArray[fileRepProcIndex] = FileRep_ShmemInitOneBuffer("filerep primary mirror messages",
																	 FileRep_ShmemSize(),
																	 FileRep_ShmemSize() - sizeof(FileRepShmem_s));

	fileRepShmemArray[fileRepProcIndex]->ipcArrayIndex = IndexIpcArrayShmem;

}

/*
 * FileRepAck_ShmemInit()
 *			Allocate and initialize FileRepAck Shared Memory
 *
 */
void
FileRepAck_ShmemInit(void)
{

	//TODO merge with FileRep_ShmemInit
	fileRepAckShmemArray[fileRepProcIndex] = FileRep_ShmemInitOneBuffer("filerep ack primary mirror messages",
																		FileRepAck_ShmemSize(),
																		FileRepAck_ShmemSize() - sizeof(FileRepShmem_s));

	fileRepAckShmemArray[fileRepProcIndex]->ipcArrayIndex = IndexIpcArrayAckShmem;
}

/*
 * FileRep_ShmemSize()
 *		Calculate size required for shared memory for mirroring
 */
Size
FileRep_ShmemSize(void)
{
	Size	size;

	size = sizeof(FileRepShmem_s);

	size = add_size(size, file_rep_buffer_size);
	size = mul_size(size, FILEREP_SHMEM_MAX_SLOTS);

	return size;
}

/*
 * FileRepAck_ShmemSize()
 *		Calculate size required for shared memory for acknowledgement
 * NOTES add call to ipci.c (grep BgWriter)
 */
Size
FileRepAck_ShmemSize(void)
{
	Size	size;

	/* shared memory for acknowledgement */
	size = sizeof(FileRepShmem_s);

	size = add_size(size, file_rep_ack_buffer_size);
	size = mul_size(size, FILEREP_ACKSHMEM_MAX_SLOTS);

	return size;
}

static void
FileRep_ShmemInitArray(
					   FileRepShmem_s	**array,
					   int				arrayCount,
					   int				bufferSize,
					   bool				initIpcArrayIndex)
{
	int	ii;

	for (ii = 0; ii < arrayCount; ii++) {

		array[ii]->positionBegin = ((char *) array[ii]) + sizeof(FileRepShmem_s);

		array[ii]->positionEnd = array[ii]->positionBegin + bufferSize;

		array[ii]->positionInsert = array[ii]->positionBegin;

		array[ii]->positionConsume = array[ii]->positionBegin;

		array[ii]->positionWraparound = array[ii]->positionEnd;

		array[ii]->insertCount = 0;

		array[ii]->consumeCount = 0;

		array[ii]->state = FileRepStateNotInitialized;

		if (initIpcArrayIndex)
		{
			array[ii]->ipcArrayIndex = ii+1;
		}
		else
		{
			switch (ii)
			{
				case 0:
					array[ii]->ipcArrayIndex = IndexIpcArrayAckShmem;
					break;

				case 1:
					array[ii]->ipcArrayIndex = IndexIpcArrayVerify;
					break;

				default:
					Assert(0);
			}
		}

		if ((ii + 1) < arrayCount)
		{
			array[ii+1] = (FileRepShmem_s *)array[ii]->positionEnd;
		}
	}

	return;
}

static void
FileRep_ShmemReInitOneBuffer(FileRepShmem_s *result, int shmemSize, int bufSize)
{

	Assert( shmemSize >= bufSize + sizeof(FileRepShmem_s));

	if (result == NULL) {
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 (errmsg("shared memory for mirroring not initialized."))));
	}


	MemSet(result, 0, sizeof(FileRepShmem_s));

	result->positionBegin = ((char *) result) + sizeof(FileRepShmem_s);

	MemSet(result->positionBegin, 0, bufSize);

	result->positionEnd = result->positionBegin + bufSize;

	result->positionInsert = result->positionBegin;

	result->positionConsume = result->positionBegin;

	result->positionWraparound = result->positionEnd;

	result->insertCount = 0;

	result->consumeCount = 0;

	result->state = FileRepStateNotInitialized;

	/*
	 * positionWraparound is initialized
	 * when message is inserted at the positionBegin.
	 */
}

/*
 * FileRep_ShmemReInit()
 *			Allocate and initialize FileRep Shared Memory
 *
 */
static void
FileRep_ShmemReInit(void)
{
		/* This function is primarily used to initialize ShMem when it is used as one pool rather
		   than divided into slots  - this initializes the single pool or slot appropriately
		   fileRepProcIndex should always be 0 here and so could be removed ie

		   fileRepShmemArray[fileRepProcIndex] replaced with *fileRepShmemArray

		   We could also merge this with FileRepAck_ShmemReInit and just pass in fileRepShmemArray or fileRepAckShmemArray as a parameter
		   somewhat like FileRep_ShmemInitArray
		*/
	Assert(fileRepProcIndex ==0);
	FileRep_ShmemReInitOneBuffer(fileRepShmemArray[fileRepProcIndex],
								 FileRep_ShmemSize(),
								 FileRep_ShmemSize() - sizeof(FileRepShmem_s));

	fileRepShmemArray[fileRepProcIndex]->ipcArrayIndex = IndexIpcArrayShmem;

	fileRepIpcArray[fileRepShmemArray[fileRepProcIndex]->ipcArrayIndex]->refCountSemC = 0;
	fileRepIpcArray[fileRepShmemArray[fileRepProcIndex]->ipcArrayIndex]->refCountSemP = 0;

	PGSemaphoreReset(&fileRepIpcArray[fileRepShmemArray[fileRepProcIndex]->ipcArrayIndex]->semC);
	PGSemaphoreReset(&fileRepIpcArray[fileRepShmemArray[fileRepProcIndex]->ipcArrayIndex]->semP);

}

/*
 * FileRepAck_ShmemReInit()
 *			Allocate and initialize FileRepAck Shared Memory
 *
 */
static void
FileRepAck_ShmemReInit(void)
{
		/* This function is primarily used to initialize AckShMem when it is used as one pool rather
		   than divided into slots  - this initializes the single pool or slot appropriately
		   fileRepProcIndex should always be 0 here and so could be removed

		   We could also merge this with FileRep_ShmemReInit and just pass in fileRepShmemArray or fileRepAckShmemArray as a parameter
		   somewhat like FileRep_ShmemInitArray
		*/
	FileRep_ShmemReInitOneBuffer(*fileRepAckShmemArray,
								 FileRepAck_ShmemSize(),
								 FileRepAck_ShmemSize() - sizeof(FileRepShmem_s));

	(*fileRepAckShmemArray)->ipcArrayIndex = IndexIpcArrayAckShmem;

	fileRepIpcArray[(*fileRepAckShmemArray)->ipcArrayIndex]->refCountSemC = 0;
	fileRepIpcArray[(*fileRepAckShmemArray)->ipcArrayIndex]->refCountSemP = 0;

	PGSemaphoreReset(&fileRepIpcArray[(*fileRepAckShmemArray)->ipcArrayIndex]->semC);
	PGSemaphoreReset(&fileRepIpcArray[(*fileRepAckShmemArray)->ipcArrayIndex]->semP);

}

/*
 * Ensure next insertPosition is also available. That will happen
 * only when insert and consume messages are aligned.
 * next insertPosition == consumePosition is TRUE when no space
 * for message insert is available.
 *
 * TEST SCENARIOS
 *		a) wraparound that is/ is not aligned with endPosition
 *		b) no more space to insert message
 *		c) no more messages to consume
 *
 *     lockId is either FileRepAckShmemLock or FileRepShmemLock
 */
char*
FileRep_ReserveShmem(
					 FileRepShmem_s		*fileRepShmem,
					 uint32				msgLength,
					 uint32				*spareField,
					 FileRepOperation_e	fileRepOperation,
					 LWLockId			lockId)
{
	bool	insert = false;
	int		retry = 0;
	bool	wait = FALSE;
	char	*msgPositionInsert = NULL;
	FileRepShmemMessageDescr_s *fileRepShmemMessageDescr;

	while ((insert == false) && (FileRep_IsRetry(retry)))
	{

		LWLockAcquire(lockId, LW_EXCLUSIVE);

		if (! FileRep_IsIpcSleep(fileRepOperation))
		{
			if (wait == TRUE)
			{
				wait = FALSE;
			}
		}

		if (fileRepShmem->positionInsert > fileRepShmem->positionConsume) {
			if ((fileRepShmem->positionInsert + msgLength + sizeof(FileRepShmemMessageDescr_s)) <
				fileRepShmem->positionEnd) {
				insert = true;
			}
			else
			{
				/* Message wraparound */
				if (fileRepShmem->positionConsume != fileRepShmem->positionBegin)
				{
					fileRepShmem->positionWraparound = fileRepShmem->positionInsert;
					fileRepShmem->positionInsert = fileRepShmem->positionBegin;
					LWLockRelease(lockId);
					continue;
				}
			}
		}

		if (fileRepShmem->positionInsert < fileRepShmem->positionConsume)
		{
			if ((fileRepShmem->positionInsert + msgLength + sizeof(FileRepShmemMessageDescr_s)) <
				fileRepShmem->positionConsume)
			{
				insert = true;
			}
		}

		if (fileRepShmem->positionInsert == fileRepShmem->positionConsume)
		{
			if ((fileRepShmem->positionInsert + msgLength + sizeof(FileRepShmemMessageDescr_s)) <
				fileRepShmem->positionEnd)
			{
				insert = true;
			}
			else
			{
				/* Message wraparound */
				fileRepShmem->positionWraparound = fileRepShmem->positionInsert;
				fileRepShmem->positionInsert = fileRepShmem->positionBegin;
				LWLockRelease(lockId);
				continue;
			}
		}

		if (insert == true)
		{
			if ((fileRepShmem->positionInsert +
				 msgLength + sizeof(FileRepShmemMessageDescr_s)) ==
				fileRepShmem->positionConsume)
			{

				insert = false;
			}
			else
			{
				msgPositionInsert = fileRepShmem->positionInsert;

				Assert(msgPositionInsert + msgLength + sizeof(FileRepShmemMessageDescr_s) < fileRepShmem->positionEnd);

				fileRepShmemMessageDescr = (FileRepShmemMessageDescr_s*) msgPositionInsert;

				fileRepShmemMessageDescr->messageLength = msgLength;

				fileRepShmemMessageDescr->messageState = FileRepShmemMessageStateReserved;

				fileRepShmem->insertCount++;

				*spareField = fileRepShmem->insertCount;

				fileRepShmem->positionInsert = msgPositionInsert + msgLength + sizeof(FileRepShmemMessageDescr_s);
			}
		}

		if (! FileRep_IsIpcSleep(fileRepOperation))
		{
			if (insert == FALSE)
			{
				fileRepIpcArray[fileRepShmem->ipcArrayIndex]->refCountSemP++;
				wait = TRUE;
			}
		}

		LWLockRelease(lockId);

		if (insert == false)
		{
			if (FileRepSubProcess_IsStateTransitionRequested())
			{
				break;
			}

			if (FileRep_IsIpcSleep(fileRepOperation))
			{
				FileRep_Sleep10ms(retry);

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
				FileRep_IpcWait(fileRepIpcArray[fileRepShmem->ipcArrayIndex]->semP, &fileRepIpcArray[fileRepShmem->ipcArrayIndex]->refCountSemP, lockId);
			}
		}
	}

	if (insert == false && Debug_filerep_print)
	{
			ereport(LOG,
					(errmsg("reserving filerep shared memory fail")));
	}

	return (msgPositionInsert);
}

/****************************************************************
 * ERROR REPORTING
 ****************************************************************/
int
FileRep_errcontext(void)
{

	errcontext(
			   "mirroring role '%s' "
			   "mirroring state '%s' "
			   "segment state '%s' "
			   "process name(pid) '%s(%d)' "
			   "filerep state '%s' ",
			   FileRepRoleToString[fileRepRole],
			   DataStateToString[dataState],
			   SegmentStateToString[segmentState],
			   FileRepProcessTypeToString[fileRepProcessType],
			   getpid(),
			   FileRepStateToString[FileRepSubProcess_GetState()]);

	return 0;
}

int
FileRep_errdetail_Shmem(void)
{

	errcontext(
		   "position begin '%p' "
		   "position end '%p' "
		   "position insert '%p' "
		   "position consume '%p' "
		   "position wraparound '%p' "
		   "insert count '%d' "
		   "consume count '%d' ",
			fileRepShmemArray[fileRepProcIndex]->positionBegin,
			fileRepShmemArray[fileRepProcIndex]->positionEnd,
			fileRepShmemArray[fileRepProcIndex]->positionInsert,
			fileRepShmemArray[fileRepProcIndex]->positionConsume,
			fileRepShmemArray[fileRepProcIndex]->positionWraparound,
			fileRepShmemArray[fileRepProcIndex]->insertCount,
			fileRepShmemArray[fileRepProcIndex]->consumeCount);

	return 0;
}

int
FileRep_errdetail_ShmemAck(void)
{

		//Todo - merge with FileRep_errdetail_Shmem just pass in pointer
	errcontext(
			  "position ack begin '%p' "
			  "position ack end '%p' "
			  "position ack insert '%p' "
			  "position ack consume '%p' "
			  "position ack wraparound '%p' "
			  "insert count ack '%d' "
			  "consume count ack '%d' ",
			  (*fileRepAckShmemArray)->positionBegin,
			  (*fileRepAckShmemArray)->positionEnd,
			  (*fileRepAckShmemArray)->positionInsert,
			  (*fileRepAckShmemArray)->positionConsume,
			  (*fileRepAckShmemArray)->positionWraparound,
			  (*fileRepAckShmemArray)->insertCount,
			  (*fileRepAckShmemArray)->consumeCount);

	return 0;
}

int
FileRep_errdetail(
				  FileRepIdentifier_u		fileRepIdentifier,
				  FileRepRelationType_e		fileRepRelationType,
				  FileRepOperation_e		fileRepOperation,
				  int						messageCount)
{
	char						*fileName = NULL;

	fileName = FileRep_GetFileName(
								   fileRepIdentifier,
								   fileRepRelationType);

	errdetail(
			  "identifier '%s' "
			  "operation '%s' "
			  "relation type '%s' "
			  "message count '%d' ",
			  (fileName == NULL) ? "<null>" : fileName,
			  FileRepOperationToString[fileRepOperation],
			  FileRepRelationTypeToString[fileRepRelationType],
			  messageCount);

	if (fileName) {
		pfree(fileName);
		fileName = NULL;
	}
	return 0;
}

int
FileRep_ReportRelationPath(
				  char*			filespaceLocation,
				  RelFileNode	relFileNode,
				  uint32		segmentFileNum)
{
	char		path[MAXPGPATH+1];

	FormRelationPath(
					 path,
					 filespaceLocation,
					 relFileNode);

	if (segmentFileNum != 0)
	{
		sprintf(path, "%s.%u", path, segmentFileNum);
	}

	errdetail("relation path '%s' ", path);

	return 0;
}

/****************************************************************
 * FILEREP IN SHARED MEMORY CONFIGURATION LOG
 ****************************************************************/
/*
 * Wraparound configuration log for troubleshooting is maintained by each segment.
 */
typedef struct FileRepLogShmem_s
{
	slock_t		logLock;

	char		*logBegin;

	char		*logEnd;

	char		*logInsert;

	bool		activeSlot1;

	bool		flushSlot1;

	bool		flushSlot2;

} FileRepLogShmem_s;

static	volatile FileRepLogShmem_s *fileRepConfigLogShmem = NULL;
static	volatile FileRepLogShmem_s *fileRepLogShmem = NULL;


static void FileRep_FlushLogActiveSlot(void);

static void FileRep_FlushLog(
							 char		*data,
							 uint32		dataLength,
							 bool		isConfigLog);

static void FileRepLog_ShmemReInit(void);

static void FileRep_CheckFlushLogs(
								   volatile FileRepLogShmem_s	*logShmem,
								   bool							isConfigLog);

static void
FileRep_CheckFlushLogs(
					   volatile	FileRepLogShmem_s	*logShmem,
					   bool							isConfigLog)
{

	if (log_filerep_to_syslogger)
		return;

	uint32	dataLength = 0;
	bool	flushConfigLog = FALSE;
	char	*configBegin = NULL;

	dataLength = (logShmem->logEnd - logShmem->logBegin) / 2;

	SpinLockAcquire(&logShmem->logLock);

	if (logShmem->flushSlot2 == TRUE)
	{
		flushConfigLog = TRUE;

		logShmem->flushSlot2 = FALSE;

		configBegin = logShmem->logBegin + dataLength;
	}

	if (logShmem->flushSlot1 == TRUE)
	{
		flushConfigLog = TRUE;

		logShmem->flushSlot1 = FALSE;

		configBegin = logShmem->logBegin;
	}

	SpinLockRelease(&logShmem->logLock);

	if (flushConfigLog == TRUE)
	{
		Assert(configBegin != NULL);

		if (configBegin != NULL)
			FileRep_FlushLog(configBegin, dataLength, isConfigLog /* isConfigLog */);
		flushConfigLog = FALSE;
	}
}

void
FileRep_InsertLogEntry(
					char*					routineName,
					FileRepIdentifier_u		fileRepIdentifier,
					FileRepRelationType_e	fileRepRelationType,
					FileRepOperation_e		fileRepOperation,
					pg_crc32				fileRepMessageHeaderCrc,
					pg_crc32				fileRepMessageBodyCrc,
					FileRepAckState_e		fileRepAckState,
					uint32					localCount,
					uint32					fileRepMessageCount)
{
	char		temp[128];
	char		buftime[128];
	char		buffer[2048];
	pg_time_t	tt;
	uint32		halfLength = 0;
	char		*fileName = NULL;

	struct timeval	logTime;

	if (!Debug_filerep_print || (*fileRepAckShmemArray) == NULL)
		return;

	if (log_filerep_to_syslogger)
	{
		ereport(LOG,
		(errmsg("'%s', "
			"filerep operation '%s', "
			"relation type '%s', "
			"'%d', '%d', "
			"header crc '%u', "
			"body crc '%u', "
			"ack state '%s', "
			"'%p', '%p', '%p', '%p', '%p', '%d', '%d', "
			"'%p', '%p', '%p', '%p', '%p', '%d', '%d', "
			"mirroring role '%s', mirroring state '%s', segment state '%s', filerep state, '%s'"
			"process name(pid) '%s(%d)' ",
			routineName,
			FileRepOperationToString[fileRepOperation],
			FileRepRelationTypeToString[fileRepRelationType],
			localCount,
			fileRepMessageCount,
			fileRepMessageHeaderCrc,
			fileRepMessageBodyCrc,
			FileRepAckStateToString[fileRepAckState],
			fileRepShmemArray[fileRepProcIndex]->positionBegin,
			fileRepShmemArray[fileRepProcIndex]->positionEnd,
			fileRepShmemArray[fileRepProcIndex]->positionInsert,
			fileRepShmemArray[fileRepProcIndex]->positionConsume,
			fileRepShmemArray[fileRepProcIndex]->positionWraparound,
			fileRepShmemArray[fileRepProcIndex]->insertCount,
			fileRepShmemArray[fileRepProcIndex]->consumeCount,
			(*fileRepAckShmemArray)->positionBegin,
			(*fileRepAckShmemArray)->positionEnd,
			(*fileRepAckShmemArray)->positionInsert,
			(*fileRepAckShmemArray)->positionConsume,
			(*fileRepAckShmemArray)->positionWraparound,
			(*fileRepAckShmemArray)->insertCount,
			(*fileRepAckShmemArray)->consumeCount,
			FileRepRoleToString[fileRepRole],
			DataStateToString[dataState],
			SegmentStateToString[segmentState],
			FileRepStateToString[FileRepSubProcess_GetState()],
			FileRepProcessTypeToString[fileRepProcessType],
			getpid())));

			return;
	}

	halfLength = (fileRepLogShmem->logEnd - fileRepLogShmem->logBegin) / 2;

	gettimeofday(&logTime, NULL);

	tt = (pg_time_t) logTime.tv_sec;
	pg_strftime(temp, sizeof(temp), "%a %b %d %H:%M:%S.%%06d %Y %Z",
				pg_localtime(&tt, session_timezone));
	snprintf(buftime, sizeof(buftime), temp, logTime.tv_usec);

	fileName = FileRep_GetFileName(
								   fileRepIdentifier,
								   fileRepRelationType);

	sprintf(buffer,
			"\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%d\",\"%d\",\"%u\",\"%u\",\"%s\",\"%p\",\"%p\",\"%p\",\"%p\",\"%p\",\"%d\",\"%d\",\"%p\",\"%p\",\"%p\",\"%p\",\"%p\",\"%d\",\"%d\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%d\"  \n",
			buftime,
			routineName,												//1
			(fileName == NULL) ? "<null>" : fileName,
			FileRepOperationToString[fileRepOperation],					//3
			FileRepRelationTypeToString[fileRepRelationType],
			localCount,													//5
			fileRepMessageCount,
			fileRepMessageHeaderCrc,									//7
			fileRepMessageBodyCrc,
			FileRepAckStateToString[fileRepAckState],					//9
			fileRepShmemArray[fileRepProcIndex]->positionBegin,
			fileRepShmemArray[fileRepProcIndex]->positionEnd,			//11
			fileRepShmemArray[fileRepProcIndex]->positionInsert,
			fileRepShmemArray[fileRepProcIndex]->positionConsume,		//13
			fileRepShmemArray[fileRepProcIndex]->positionWraparound,
			fileRepShmemArray[fileRepProcIndex]->insertCount,			//15
			fileRepShmemArray[fileRepProcIndex]->consumeCount,
			(*fileRepAckShmemArray)->positionBegin,						//17
			(*fileRepAckShmemArray)->positionEnd,
			(*fileRepAckShmemArray)->positionInsert,					//19
			(*fileRepAckShmemArray)->positionConsume,
			(*fileRepAckShmemArray)->positionWraparound,				//21
			(*fileRepAckShmemArray)->insertCount,
			(*fileRepAckShmemArray)->consumeCount,						//23
			FileRepRoleToString[fileRepRole],
			DataStateToString[dataState],								//25
			SegmentStateToString[segmentState],
			FileRepStateToString[FileRepSubProcess_GetState()],			//27
			FileRepProcessTypeToString[fileRepProcessType],
			getpid());													//29

	SpinLockAcquire(&fileRepLogShmem->logLock);

	if ((fileRepLogShmem->logInsert + strlen(buffer)) >= fileRepLogShmem->logEnd)
	{
		fileRepLogShmem->logInsert = fileRepLogShmem->logBegin;

		fileRepLogShmem->activeSlot1 = TRUE;

		fileRepLogShmem->flushSlot2 = TRUE;
	}

	if (fileRepLogShmem->logInsert >= fileRepLogShmem->logBegin + halfLength)
	{
		fileRepLogShmem->activeSlot1 = FALSE;

		fileRepLogShmem->flushSlot1 = TRUE;
	}

	memcpy(fileRepLogShmem->logInsert, buffer, strlen(buffer));

	fileRepLogShmem->logInsert = fileRepLogShmem->logInsert + strlen(buffer);

	SpinLockRelease(&fileRepLogShmem->logLock);

	if (fileName) {
		pfree(fileName);
		fileName = NULL;
	}
}

/*
 * FileRepLog_ShmemSize()
 *		Calculate size required for configuration log
 */
Size
FileRepLog_ShmemSize(void)
{
	Size	size;

	size = sizeof(FileRepLogShmem_s);

	size = add_size(size, file_rep_ack_buffer_size); // 512kB

	/* double size, one slot for config log and another slot for messages */
	size = mul_size(size, 2);

	return size;
}


/*
 * FileRepLog_ShmemInit()
 *			Allocate and initialize FileRep Shared Memory
 *
 */
void
FileRepLog_ShmemInit(void)
{
	bool	foundPtr;

	fileRepConfigLogShmem = (FileRepLogShmem_s *) ShmemInitStruct("filerep config log",
																	FileRepLog_ShmemSize()/2,
																	&foundPtr);

	if (fileRepConfigLogShmem == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 (errmsg("not enough shared memory to log filerep config"))));
	}

	if (! foundPtr)
	{
		MemSet(fileRepConfigLogShmem, 0, sizeof(FileRepLogShmem_s));
	}

	SpinLockInit(&fileRepConfigLogShmem->logLock);

	fileRepConfigLogShmem->logBegin = ((char*) fileRepConfigLogShmem) + sizeof(FileRepLogShmem_s);

	fileRepConfigLogShmem->logInsert = fileRepConfigLogShmem->logBegin;

	fileRepConfigLogShmem->logEnd = fileRepConfigLogShmem->logBegin + file_rep_ack_buffer_size;

	MemSet(fileRepConfigLogShmem->logBegin, 0, file_rep_ack_buffer_size);

	fileRepConfigLogShmem->activeSlot1 = TRUE;

	fileRepConfigLogShmem->flushSlot1 = FALSE;

	fileRepConfigLogShmem->flushSlot2 = FALSE;



	fileRepLogShmem = (FileRepLogShmem_s *) ShmemInitStruct("filerep log",
															FileRepLog_ShmemSize()/2,
															&foundPtr);

	if (fileRepLogShmem == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 (errmsg("not enough shared memory to log filerep config"))));
	}

	if (! foundPtr)
	{
		MemSet(fileRepLogShmem, 0, sizeof(FileRepLogShmem_s));
	}

	SpinLockInit(&fileRepLogShmem->logLock);

	fileRepLogShmem->logBegin = ((char*) fileRepLogShmem) + sizeof(FileRepLogShmem_s);

	fileRepLogShmem->logInsert = fileRepLogShmem->logBegin;

	fileRepLogShmem->logEnd = fileRepLogShmem->logBegin + file_rep_ack_buffer_size;

	MemSet(fileRepLogShmem->logBegin, 0, file_rep_ack_buffer_size);

	fileRepLogShmem->activeSlot1 = TRUE;

	fileRepLogShmem->flushSlot1 = FALSE;

	fileRepLogShmem->flushSlot2 = FALSE;
}

static void
FileRepLog_ShmemReInit(void)
{

	if (fileRepConfigLogShmem == NULL || fileRepLogShmem == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 (errmsg("not enough shared memory to log filerep config"))));
	}

	MemSet(fileRepConfigLogShmem, 0, sizeof(FileRepLogShmem_s));

	SpinLockInit(&fileRepConfigLogShmem->logLock);

	fileRepConfigLogShmem->logBegin = ((char*) fileRepConfigLogShmem) + sizeof(FileRepLogShmem_s);

	fileRepConfigLogShmem->logInsert = fileRepConfigLogShmem->logBegin;

	fileRepConfigLogShmem->logEnd = fileRepConfigLogShmem->logBegin + file_rep_ack_buffer_size;

	MemSet(fileRepConfigLogShmem->logBegin, 0, file_rep_ack_buffer_size);

	fileRepConfigLogShmem->activeSlot1 = TRUE;

	fileRepConfigLogShmem->flushSlot1 = FALSE;

	fileRepConfigLogShmem->flushSlot2 = FALSE;


	MemSet(fileRepLogShmem, 0, sizeof(FileRepLogShmem_s));

	SpinLockInit(&fileRepLogShmem->logLock);

	fileRepLogShmem->logBegin = ((char*) fileRepLogShmem) + sizeof(FileRepLogShmem_s);

	fileRepLogShmem->logInsert = fileRepLogShmem->logBegin;

	fileRepLogShmem->logEnd = fileRepLogShmem->logBegin + file_rep_ack_buffer_size;

	MemSet(fileRepLogShmem->logBegin, 0, file_rep_ack_buffer_size);

	fileRepLogShmem->activeSlot1 = TRUE;

	fileRepLogShmem->flushSlot1 = FALSE;

	fileRepLogShmem->flushSlot2 = FALSE;
}

static void
FileRep_FlushLogActiveSlot(void)
{
	if (log_filerep_to_syslogger)
		return;

	uint32 dataLength = 0;

	dataLength = (fileRepConfigLogShmem->logEnd - fileRepConfigLogShmem->logBegin) / 2;

	if (Debug_filerep_config_print)
	{
		if (fileRepConfigLogShmem->activeSlot1 == TRUE)
		{
			/* flush what is inserted in slot1 */
			FileRep_FlushLog(fileRepConfigLogShmem->logBegin,
							 fileRepConfigLogShmem->logInsert - fileRepConfigLogShmem->logBegin,
							 TRUE /* isConfigLog */);
		}
		else
		{
			/* flush what is inserted in slot2 */
			FileRep_FlushLog(fileRepConfigLogShmem->logBegin + dataLength,
							 fileRepConfigLogShmem->logInsert - fileRepConfigLogShmem->logBegin + dataLength,
							 TRUE /* isConfigLog */);
		}
	}

	dataLength = (fileRepLogShmem->logEnd - fileRepLogShmem->logBegin) / 2;

	if (Debug_filerep_print)
	{
		if (fileRepLogShmem->activeSlot1 == TRUE)
		{
			/* flush what is inserted in slot1 */
			FileRep_FlushLog(fileRepLogShmem->logBegin,
							 fileRepLogShmem->logInsert - fileRepLogShmem->logBegin,
							 FALSE /* isConfigLog */);
		}
		else
		{
			/* flush what is inserted in slot2 */
			FileRep_FlushLog(fileRepLogShmem->logBegin + dataLength,
							 fileRepLogShmem->logInsert - fileRepLogShmem->logBegin + dataLength,
							 FALSE /* isConfigLog */);
		}
	}
}

static void
FileRep_FlushLog(
					   char		*data,
					   uint32	dataLength,
					   bool		isConfigLog)
{
	File	file = 0;
	char	path[MAXPGPATH+1];
	int64	position = 0;

	uint32	dataLengthLocal = 0;

	if (isConfigLog)
	{
		FileRepConfigLogPath(path);
	}
	else
	{
		FileRepLogPath(path);
	}

	errno = 0;
	/* open it (and create if doesn't exist) */
	file = 	PathNameOpenFile(path,
							 O_RDWR | O_CREAT | PG_BINARY,
							 S_IRUSR | S_IWUSR);

	if (file < 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("could not open file '%s' : %m",
						path)));
		return;

	}

	position = FileSeek(file, 0, SEEK_END);

	/*
	 * seek to beginning of file. The meta file only has a single
	 * block. we will overwrite it each time with new meta data.
	 */
	if (position > FILEREP_LOG_MAX_SIZE)
	{
		if (FileSeek(file, 0, SEEK_SET) != 0)
		{
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("could not seek in file to position '0' in file '%s' : %m",
							path)));
			return;
		}
	}

	while (dataLength > 0)
	{
		dataLengthLocal = Min(FILEREP_MESSAGEBODY_LEN, dataLength);

		if ((int) FileWrite(file,
							(char*) data,
							dataLengthLocal) != dataLengthLocal)
		{
			if (errno == 0)
				errno = ENOSPC;

			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("could not write to file '%s' data '%p' dataLength '%u' : %m",
							path,
							data,
							dataLengthLocal)));
			return;
		}

		data = (char *) data + dataLengthLocal;

		dataLength -= dataLengthLocal;
	}

	if (FileSync(file) != 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("could not fsync file '%s' : %m", path)));
		return;
	}

	FileClose(file);

}

/*
 *
 *
 */
void
FileRep_InsertConfigLogEntryInternal(char		*description,
									 const char *fileName,
									 int		lineNumber,
									 const char *funcName)
{
	char		temp[128];
	char		buftime[128];
	char		buffer[2048];
	pg_time_t	tt;

	struct timeval	logTime;
	uint32		halfLength = (fileRepConfigLogShmem->logEnd - fileRepConfigLogShmem->logBegin) / 2;

	if (!Debug_filerep_config_print)
		return;

	if (log_filerep_to_syslogger)
	{
		ereport(LOG,
				(errmsg("'%s', "
						"mirroring role '%s' "
						"mirroring state '%s' "
						"segment state '%s' "
						"filerep state '%s' "
						"process name(pid) '%s(%d)' "
						"'%s' 'L%d' '%s' ",
						description,
						FileRepRoleToString[fileRepRole],
						DataStateToString[dataState],
						SegmentStateToString[segmentState],
						FileRepStateToString[FileRepSubProcess_GetState()],
						FileRepProcessTypeToString[fileRepProcessType],
						getpid(),
						fileName,
						lineNumber,
						funcName)));
		return;
	}

	SpinLockAcquire(&fileRepConfigLogShmem->logLock);

	gettimeofday(&logTime, NULL);

	tt = (pg_time_t) logTime.tv_sec;
	pg_strftime(temp, sizeof(temp), "%a %b %d %H:%M:%S.%%06d %Y %Z",
				pg_localtime(&tt, session_timezone));
	snprintf(buftime, sizeof(buftime), temp, logTime.tv_usec);

	sprintf(buffer, "\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%d\",\"%s L%d %s\" \n",
			buftime,
			description,
			FileRepRoleToString[fileRepRole],
			DataStateToString[dataState],
			SegmentStateToString[segmentState],
			FileRepStateToString[FileRepSubProcess_GetState()],
			FileRepProcessTypeToString[fileRepProcessType],
			getpid(),
			fileName,
			lineNumber,
			funcName);

	if ((fileRepConfigLogShmem->logInsert + strlen(buffer)) >= fileRepConfigLogShmem->logEnd)
	{
		fileRepConfigLogShmem->logInsert = fileRepConfigLogShmem->logBegin;

		fileRepConfigLogShmem->activeSlot1 = TRUE;

		fileRepConfigLogShmem->flushSlot2 = TRUE;
	}

	if (fileRepConfigLogShmem->logInsert >= fileRepConfigLogShmem->logBegin + halfLength)
	{
		fileRepConfigLogShmem->activeSlot1 = FALSE;

		fileRepConfigLogShmem->flushSlot1 = TRUE;
	}

	memcpy(fileRepConfigLogShmem->logInsert, buffer, strlen(buffer));

	fileRepConfigLogShmem->logInsert = fileRepConfigLogShmem->logInsert + strlen(buffer);

	SpinLockRelease(&fileRepConfigLogShmem->logLock);
}

/****************************************************************
 *  FILEREP PROCESS
 ****************************************************************/

/*
 *  SIGHUP signal from postmaster
 *  It re-loads configuration file at next convenient time.
 */
static void
FileRep_SigHupHandler(SIGNAL_ARGS)
{
	reloadConfigFile = true;
	elog(FILEREP_PROCESS_LOG_LEVEL, "FileRep_SigHupHandler()");
}

/*
 *  SIGQUIT signal from postmaster
 */
static void
FileRep_ImmediateShutdownHandler(SIGNAL_ARGS)
{
	immediateShutdownRequested = true;
	elog(FILEREP_PROCESS_LOG_LEVEL, "FileRep_ImmediateShutdownHandler()");
}

/*
 *  SIGUSR2 signal from postmaster
 */
static void
FileRep_ShutdownHandler(SIGNAL_ARGS)
{
	shutdownRequested = true;
	elog(FILEREP_PROCESS_LOG_LEVEL, "FileRep_ShutdownHandler()");
}

/*
 *  SIGCHLD signal from FileRep process children
 */
static void
FileRep_ChildTerminationHandler(SIGNAL_ARGS)
{
	childTermination = true;
	elog(FILEREP_PROCESS_LOG_LEVEL, "FileRep_ChildTerminationHandler()");
}

/*
 *  SIGUSR1 signal from postmaster
 *  It signals about data and/or segment state change.
 */
static void
FileRep_StateHandler(SIGNAL_ARGS)
{
	++stateChangeRequestCounter;
	elog(FILEREP_PROCESS_LOG_LEVEL, "FileRep_StateHandler() stateChangeRequestCounter %d", stateChangeRequestCounter);
}

static void
FileRep_HandleCrash(SIGNAL_ARGS)
{
	StandardHandlerForSigillSigsegvSigbus_OnMainThread("a file replication process", PASS_SIGNAL_ARGS);
}

/*
 * NOTE - should we report fault
 *
 * @param backendsOnly if true then only filerep processes that act as backends will be signalled.
 */
void
FileRep_SignalChildren(int signal, bool backendsOnly)
{
	int ii;

	/*
	 * Only postmaster or filerep main process can call this.
	 * See also the comment on FileRepSubProcList.
	 */
	Assert(!IsUnderPostmaster || fileRepProcessType == FileRepProcessTypeMain);

	for (ii=0; ii < MaxFileRepSubProc; ii++) {
		FileRepSubProc *subProc = &FileRepSubProcList[ii];

		if (subProc->pid <= 0) {
			continue;
		}

        if ( backendsOnly &&
             ! FileRepIsBackendSubProcess( subProc->fileRepProcessType)) {
            continue;
        }

        if (kill(subProc->pid, signal) < 0)  {
            ereport(WARNING,
                    (errmsg("signal to children failed pid '%ld' signal '%d' : %m",
                     (long) subProc->pid, signal)));

        }
	}
	return;
}

static bool
FileRep_IsAnyChildAlive(void)
{
	int i;

	for (i=0; i < MaxFileRepSubProc; i++)
	{
		if ( FileRepSubProcList[i].pid != 0 )
			return true;
	}
	return false;
}

static bool
FileRep_IsAnyBackendChildAlive(void)
{
	int i;

	for (i=0; i < MaxFileRepSubProc; i++)
	{
		if ( FileRepSubProcList[i].pid != 0 &&
		     FileRepIsBackendSubProcess(FileRepSubProcList[i].fileRepProcessType))
        {
		    return true;
        }
	}
	return false;
}

/*
 * Log the death of a child process.
 */
static void
LogChildExit(int lev, const char *procname, int pid, int exitstatus)
{
	if (WIFEXITED(exitstatus))
		ereport(lev,

				/*
		 translator: %s is a noun phrase describing a child process, such as
		 "server process" */
				(errmsg("%s (PID %d) exited with exit code %d",
						procname,
						pid, WEXITSTATUS(exitstatus))));
	else if (WIFSIGNALED(exitstatus))
#if defined(WIN32)
		ereport(lev,

				/*
		 translator: %s is a noun phrase describing a child process, such as
		 "server process" */
				(errmsg("%s (PID %d) was terminated by exception 0x%X",
						procname,
						pid, WTERMSIG(exitstatus)),
				 errhint("See C include file \"ntstatus.h\" for a description of the hexadecimal value.")));
#elif defined(HAVE_DECL_SYS_SIGLIST) && HAVE_DECL_SYS_SIGLIST
	ereport(lev,

			/*
			 * translator: %s is a noun phrase describing a child process,
			 * such as "server process".
			 *
			 * strsignal() is preferred over the deprecated use of
			 * sys_siglist, on platforms that support it.  Solaris and Linux
			 * do support it, but I think MAC OSX doesn't?
			 */
			(errmsg("%s (PID %d) was terminated by signal %d: %s",
					procname,
					pid, WTERMSIG(exitstatus),
					WTERMSIG(exitstatus) < NSIG ?
					sys_siglist[WTERMSIG(exitstatus)] : "(unknown)")));
#else
	{
		// If we don't have strsignal or sys_siglist, do our own translation
		const char *signalName;

		switch (WTERMSIG(exitstatus))
		{
			case SIGINT:
				signalName = "SIGINT";
				break;
			case SIGTERM:
				signalName = "SIGTERM";
				break;
			case SIGQUIT:
				signalName = "SIGQUIT";
				break;
			case SIGSTOP:
				signalName = "SIGSTOP";
				break;
			case SIGUSR1:
				signalName = "SIGUSR1";
				break;
			case SIGUSR2:
				signalName = "SIGUSR2";
				break;
			case SIGILL:
				signalName = "SIGILL";
				break;
			case SIGABRT:
				signalName = "SIGABRT";
				break;
			case SIGBUS:
				signalName = "SIGBUS";
				break;
			case SIGSEGV:
				signalName = "SIGSEGV";
				break;
			case SIGKILL:
				signalName = "SIGKILL";
				break;
			default:
				signalName = "(unknown)";
				break;
		}

		ereport(lev,

				/*
		 translator: %s is a noun phrase describing a child process, such as
		 "server process" */
			    (errmsg("%s (PID %d) was terminated by signal %d: %s",
						procname,
						pid, WTERMSIG(exitstatus), signalName)));
	}
#endif
	else
		ereport(lev,

				/*
		 translator: %s is a noun phrase describing a child process, such as
		 "server process" */
				(errmsg("%s (PID %d) exited with unrecognized status %d",
						FileRepProcessTypeToString[fileRepProcessType],
						pid, exitstatus)));

	{
		char	tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];

		snprintf(tmpBuf, sizeof(tmpBuf), "process exit, process name '%s' process pid '%d' exit status '%d' ",
				 procname,
				 pid,
				 WEXITSTATUS(exitstatus));

		FileRep_InsertConfigLogEntry(tmpBuf);
	}
}

/*
 *  FileRep_ProcessSignals()
 *
 */
static bool
FileRep_ProcessSignals()
{
	bool processExit = false;

	if (reloadConfigFile)
	{
		bool	temp = Debug_filerep_memory_log_flush;

		FileRep_SignalChildren(SIGHUP, false);
		reloadConfigFile = false;
		ProcessConfigFile(PGC_SIGHUP);

		FileRep_SetFileRepRetry();

		if (temp == FALSE &&
			Debug_filerep_memory_log_flush == TRUE)
		{
			FileRep_FlushLogActiveSlot();
		}
	}

	if (shutdownRequested)
	{
	    SegmentState_e segmentState;
		getPrimaryMirrorStatusCodes(NULL, &segmentState, NULL, NULL);

		shutdownRequested = false;

		if ( segmentState == SegmentStateShutdownFilerepBackends )
		{
            elog(FILEREP_PROCESS_LOG_LEVEL, "Filerep backends shutdown requested");
            FileRep_SetState(FileRepStateShutdownBackends);
    		FileRep_SignalChildren(SIGUSR2, true);
		}
		else
		{
            elog(FILEREP_PROCESS_LOG_LEVEL, "Filerep shutdown requested");

            /* inform mirror that gracefull shutdown is in progress */
            if (fileRepRole == FileRepPrimaryRole)
            {
                if (MyProc == NULL)
                {
                    InitAuxiliaryProcess();

                    InitBufferPoolBackend();
                }

                FileRepPrimary_MirrorShutdown();
            }

    		processExit = true;
    		FileRep_SetState(FileRepStateShutdown);
    		FileRep_SignalChildren(SIGUSR2, false);
		}
	}

	if (immediateShutdownRequested)
	{
	    SegmentState_e segState;

		immediateShutdownRequested = false;
	    elog(FILEREP_PROCESS_LOG_LEVEL, "Filerep Immediate shutdown requested");
		getPrimaryMirrorStatusCodes(NULL, &segState, NULL, NULL);

		/*
		 * Someone told me to shutdown immediately, but if segment state
		 * doesn't agree let's tell postmaster try so.  We must not send
		 * SIGQUIT to children if this doesn't agree, because then mirror
		 * will be confused and finally its postmaster will die.  The only
		 * guy who can tell SIGQUIT correctly is postmaster, and he should
		 * correctly set segment state.  See signal_filerep_to_shutdown().
		 * If postmaster doesn't exist and we get this message, anyway the
		 * next PostmasterIsAlive check will lead to the process exit.
		 */
		if (segState != SegmentStateImmediateShutdown)
			FileRep_SetPostmasterReset();
		else
		{
			SIMPLE_FAULT_INJECTOR(FileRepImmediateShutdownRequested);

			FileRep_SignalChildren(SIGQUIT, false);
			processExit = true;
			exitWithImmediateShutdown = true;
		}
	}

	/*
	 * Immediate shutdown if postmaster is not alive to avoid
	 * manual cleanup.
	 */
	if (! PostmasterIsAlive(true))
	{
	    elog(FILEREP_PROCESS_LOG_LEVEL, "Filerep: postmaster is gone, entering immediate shutdown");
		FileRep_SignalChildren(SIGQUIT, false);
		processExit = true;
		exitWithImmediateShutdown = true;
	}

	if (childTermination)
	{
		int i;
		int term_pid;

		childTermination = false;

		for (;;)
		{
			/* Don't leave child processes defunct. */
#ifdef HAVE_WAITPID
			int term_status;
			errno = 0;
			term_pid = waitpid(-1, &term_status, WNOHANG);
#else
			union wait term_status;
			errno = 0;
			term_pid = wait3(&term_status, WNOHANG, NULL);
#endif

			if (term_pid == 0)
				break;
			if ( errno == ECHILD)
				break;

			/* NOTE see do_reaper() */
			for (i=0; i < MaxFileRepSubProc; i++)
			{
				if (term_pid == FileRepSubProcList[i].pid)
				{
					LogChildExit(Debug_filerep_print ? LOG : DEBUG1,
								 (FileRepProcessTypeToString[i]),
								 FileRepSubProcList[i].pid,
								 term_status);

					if (EXIT_STATUS_0(term_status))
					{
						switch (i)
						{
							case FileRepProcessTypeResyncManager:
							case FileRepProcessTypeResyncWorker1:
							case FileRepProcessTypeResyncWorker2:
							case FileRepProcessTypeResyncWorker3:
							case FileRepProcessTypeResyncWorker4:

								getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);
								if (dataState != DataStateInSync)
								{
									FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
								}
								break;
							case FileRepProcessTypePrimaryRecovery:

								getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);
								if (dataState == DataStateInSync && segmentState == SegmentStateInitialization)
								{
									FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
								}

								if (dataState == DataStateInChangeTracking &&
									(segmentState == SegmentStateInitialization ||
									 segmentState == SegmentStateInChangeTrackingTransition))
								{
									FileRep_SetPostmasterReset();
								}

								break;
							default:
								break;
						}
					}

					if (! EXIT_STATUS_0(term_status) &&
					    ! EXIT_STATUS_1(term_status) &&
					    ! EXIT_STATUS_2(term_status))
					{
					    FileRep_SetPostmasterReset();
					}

					/*
					 * filerep processes exit with 2 if immediate shutdown or fault
					 */
					if ((EXIT_STATUS_1(term_status) || EXIT_STATUS_2(term_status) ) &&
						segmentState != SegmentStateShutdown &&
						segmentState != SegmentStateImmediateShutdown &&
						segmentState != SegmentStateShutdownFilerepBackends)
					{
						FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
					}
					FileRepSubProcList[i].pid = 0;
					break;
				}
			}
		}
		errno = 0;
	}

	for ( ;; )
	{
	    /* check to see if change required */
	    sig_atomic_t curStateChangeRequestCounter = stateChangeRequestCounter;
	    if ( curStateChangeRequestCounter == lastChangeRequestProcessCounterValue )
	        break;
        lastChangeRequestProcessCounterValue = curStateChangeRequestCounter;

		getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);

		FileRep_SignalChildren(SIGUSR1, false);
	}

	return(processExit);
}

/*
 *  FileRep_GetState()
 *  Return state of FileRep process
 */
FileRepState_e
FileRep_GetState(void)
{
	return fileRepState;
}

/* NOTE temporary routine */
bool
FileRep_IsInRecovery(void)
{
	return (segmentState == SegmentStateInitialization ||
			segmentState == SegmentStateInChangeTrackingTransition);
}

/* NOTE temporary routine */
bool
FileRep_IsInResynchronizeReady(void)
{
	return (dataState == DataStateInResync &&
		   segmentState != SegmentStateInResyncTransition);
}

/**
 * Return true if the process type represents a process that acts like
 *   a normal database backend
 */
bool
FileRepIsBackendSubProcess(FileRepProcessType_e processType)
{
    switch(processType)
    {
        case FileRepProcessTypeNotInitialized:
	    case FileRepProcessTypeMain:
	    case FileRepProcessTypePrimarySender:
	    case FileRepProcessTypeMirrorReceiver:
    	case FileRepProcessTypeMirrorConsumer:
	    case FileRepProcessTypeMirrorSenderAck:
    	case FileRepProcessTypePrimaryReceiverAck:
	    case FileRepProcessTypePrimaryConsumerAck:
	    case FileRepProcessTypeMirrorConsumerWriter:
        case FileRepProcessTypeMirrorConsumerAppendOnly1:
            return false;
		case FileRepProcessTypePrimaryRecovery:
        case FileRepProcessTypeResyncManager:
        case FileRepProcessTypeResyncWorker1:
        case FileRepProcessTypeResyncWorker2:
        case FileRepProcessTypeResyncWorker3:
        case FileRepProcessTypeResyncWorker4:
            return true;
        default:
            Assert(!"unknown process type in 'FileRepIsBackendSubProcess' ");
            return false;
    }
}

/*
 * NOTE temporary, it will be removed
 */
bool
FileRep_IsInChangeTracking(void)
{
	DataState_e snapDataState;
	bool result;

	getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);
	snapDataState = dataState;

	result = (((snapDataState == DataStateInChangeTracking &&
			    segmentState != SegmentStateChangeTrackingDisabled) ||
			   (snapDataState == DataStateInResync &&
			    !FileRep_IsInResynchronizeReady() &&
			    !isFullResync())) &&
			  !FileRep_IsInRecovery());

	return result;
}

void
FileRep_SetDataState(DataState_e dataState, bool signalPostmaster)
{
	if (updateDataState(dataState) && signalPostmaster) {
		SendPostmasterSignal(PMSIGNAL_FILEREP_STATE_CHANGE);
	}

	FileRep_InsertConfigLogEntry("set data state");
}

void
FileRep_SetSegmentState(SegmentState_e segmentState, FaultType_e faultType)
{
	/*
	 * Track mirror loss. That is required since segment state
	 * may get overwritten with other state (i.e. Shutdown, ...)
	 */
	if (segmentState == SegmentStateFault)
	{
		fileRepShmemArray[0]->state = FileRepStateFault;
	}

	if (updateSegmentState(segmentState, faultType)) {
		SendPostmasterSignal(PMSIGNAL_FILEREP_STATE_CHANGE);
	}

	FileRep_InsertConfigLogEntry("set segment state");
}

void
FileRep_SetPostmasterReset(void)
{
	SendPostmasterSignal(PMSIGNAL_POSTMASTER_RESET_FILEREP);

	FileRep_InsertConfigLogEntry("request postmaster reset");
}

/*
 *  Set state in FileRep process and sent signal to postmaster
 */
void
FileRep_SetState(FileRepState_e fileRepStateLocal)
{
	elog(FILEREP_PROCESS_LOG_LEVEL, "FileRep_SetState(%s, %d) MyProcPid:'%d'",
		 FileRepStateToString[fileRepStateLocal], fileRepStateLocal,
		 getpid());

	switch (fileRepState) {
		case FileRepStateNotInitialized:
		case FileRepStateInitialization:

			fileRepState = fileRepStateLocal;
			break;
		case FileRepStateReady:

			switch (fileRepStateLocal) {
				case FileRepStateInitialization:
				case FileRepStateFault:
				case FileRepStateShutdown:
				case FileRepStateShutdownBackends:
					fileRepState = fileRepStateLocal;
					break;
				case FileRepStateNotInitialized:
					Assert(0);
				case FileRepStateReady:

					break;
                default:
                    Assert(0);
                    break;
			}
			break;
		case FileRepStateFault:

			switch (fileRepStateLocal) {
				case FileRepStateFault:
				case FileRepStateShutdown:
				case FileRepStateShutdownBackends:
					fileRepState = fileRepStateLocal;
					break;
				case FileRepStateNotInitialized:
				case FileRepStateInitialization:
				case FileRepStateReady:
					break;
                default:
                    Assert(0);
                    break;
			}

			break;

        case FileRepStateShutdownBackends:

			switch (fileRepStateLocal) {
				case FileRepStateShutdown:
				case FileRepStateShutdownBackends:
					fileRepState = fileRepStateLocal;
					break;
				case FileRepStateNotInitialized:
				case FileRepStateInitialization:
				case FileRepStateReady:

				case FileRepStateFault:
					break;
                default:
                    Assert(0);
                    break;
			}

			break;

		case FileRepStateShutdown:

			switch (fileRepStateLocal) {
				case FileRepStateShutdown:
					fileRepState = fileRepStateLocal;
					break;
				case FileRepStateNotInitialized:
				case FileRepStateInitialization:
				case FileRepStateReady:

				case FileRepStateFault:
					break;
                default:
                    Assert(0);
                    break;
			}

			break;

        default:
            Assert(0);
            break;
	}

	switch (fileRepState) {
		case FileRepStateReady:
			FileRep_SetSegmentState(SegmentStateReady, FaultTypeNotInitialized);
			break;

		case FileRepStateFault:
			/* update shared memory configuration
			   bool updateSegmentState(FAULT);
			   return TRUE if state was updated;
			   return FALSE if state was already set to FAULT
			   change signal to PMSIGNAL_FILEREP_SEGMENT_STATE_CHANGE
			 */
			FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
			break;

		case FileRepStateInitialization:
        case FileRepStateShutdownBackends:
		case FileRepStateShutdown:
		case FileRepStateNotInitialized:
			/* No operation */
			break;
        default:
            Assert(0);
            break;
	}

	switch (FileRep_GetState())
	{
		 case FileRepStateShutdownBackends:
		 case FileRepStateShutdown:
			break;
		 default:
			FileRep_InsertConfigLogEntry("set filerep state");

			break;
	 }
}

/*
 *
 */
static void
FileRep_ConfigureSignals(void)
{

/* Accept Signals */
	/* emergency shutdown */
	pqsignal(SIGQUIT, FileRep_ImmediateShutdownHandler);

	/* graceful shutdown */
	pqsignal(SIGUSR2, FileRep_ShutdownHandler);

	/* reload configuration file */
	pqsignal(SIGHUP, FileRep_SigHupHandler);

	/* child terminated */
	pqsignal(SIGCHLD, FileRep_ChildTerminationHandler);

	/* data or segment state changed */
	pqsignal(SIGUSR1, FileRep_StateHandler);

#ifdef SIGBUS
	pqsignal(SIGBUS, FileRep_HandleCrash);
#endif
#ifdef SIGILL
	pqsignal(SIGILL, FileRep_HandleCrash);
#endif
#ifdef SIGSEGV
	pqsignal(SIGSEGV, FileRep_HandleCrash);
#endif

/* Ignore Signals */
	pqsignal(SIGTERM, SIG_IGN);
	pqsignal(SIGALRM, SIG_IGN);
	pqsignal(SIGPIPE, SIG_IGN);

/* Use default action */
	pqsignal(SIGINT, SIG_DFL);
	pqsignal(SIGTTIN, SIG_DFL);
	pqsignal(SIGTTOU, SIG_DFL);
	pqsignal(SIGCONT, SIG_DFL);
	pqsignal(SIGWINCH, SIG_DFL);

}

/*
 *
 */
static pid_t
FileRep_StartChildProcess(FileRepProcessType_e type)
{
	pid_t	pid;

	pid = fork_process();

	/* in child */
	if (pid == 0) {

		/* Release postmaster's working memory context */
		MemoryContextSwitchTo(TopMemoryContext);
		MemoryContextDelete(fileRepMemoryContext);
		fileRepMemoryContext = NULL;

		/* check if that is allowed */
		IsUnderPostmaster = TRUE;

		fileRepProcessType = type;

		FileRepSubProcess_Main();
		proc_exit(0);
	}

	/* in parent, fork failed */
	if (pid < 0) {

		ereport(WARNING,
				(errmsg("could not fork %s : %m",
						FileRepProcessTypeToString[type])));

		/* it is set in Main routine: FileRep_SetState(FileRepStateFault); */
	}

	/* Do not set failed pid. */
	if (pid > 0)
		FileRepSubProcList[type].pid = pid;
	else
		FileRepSubProcList[type].pid = 0;

	/* in parent, successful fork */
	return pid;
}

void
FileRep_SetFileRepRetry(void)
{
	if (gp_segment_connect_timeout == 0)
	{
		file_rep_retry = 0;
	}
	else
	{
		file_rep_retry = gp_segment_connect_timeout * 10 + 200;
	}

	if (Debug_filerep_print)
	{
		/*
		 * Double response time since additional tracing significantly slow down the system
		 */
		file_rep_retry *= 2;
	}

	{
		char	tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];

		snprintf(tmpBuf, sizeof(tmpBuf), "guc 'gp_segment_connect_timeout' value '%d' ",
				 gp_segment_connect_timeout);

		FileRep_InsertConfigLogEntry(tmpBuf);
	}
}

/*
 * 100 * 10ms + 100 * 50ms + 1800 * 100ms = 186 sec
 * file_rep_retry = 2000 (default value)
 */
void
FileRep_Sleep10ms(int retry)
{

	if (retry < 100) {
		pg_usleep(10000L); /* 10 ms */
	} else {
		if (retry < 200)
			pg_usleep(50000L); /* 50 ms */
		else {
			pg_usleep(100000L); /* 100 ms */
		}
	}
}

/*
 * 50 * 1ms + 50 * 10ms + 100 * 50ms + 1800 * 100ms = 185.55 sec
 * file_rep_retry = 2000 (default value)
 */
void
FileRep_Sleep1ms(int retry)
{

	if (retry < 50) {
		pg_usleep(1000L); /* 1 ms */
	} else {
		if (retry < 100) {
			pg_usleep(10000L); /* 10 ms */
		} else {
			if (retry < 200) {
				pg_usleep(50000L); /* 50 ms */
			} else {
				pg_usleep(100000L); /* 100 ms */
			}
		}
	}
}

#define ADLER_BASE 65521 /* largest prime smaller than 65536 */

/*
 * An Adler-32 checksum is obtained by calculating two 16-bit checksums A and B and concatenating their bits
 * into a 32-bit integer. A is the sum of all bytes in the string plus one, and B is the sum of the individual
 * values of A  from each step.
 *
 * At the beginning of an Adler-32 run, A is initialized to 1, B to 0. The sums are done modulo 65521
 * (the largest prime number smaller than 216). The bytes are stored in network order (big endian),
 * B occupying the two most significant bytes.
 *
 * NOTE: Adler-32 is not an actual CRC, it is a hash.  It is faster than the normal pg_crc32 routines, but
 * slower than using the Intel core-i7 "calculate crc" instruction.
 * How much faster is it than a fast crc32 like the Intel "slicing-by-8" design?
 */
static pg_crc32
FileRep_CalculateAdler32(unsigned char *buf, int len)
{
	uint32 adler = 1L;
	uint32 s1 = adler & 0xffff;
	uint32 s2 = (adler >> 16) & 0xffff;
	int n;

	for (n = 0; n < len; n++)
	{
		s1 = (s1 + buf[n]) % ADLER_BASE;
		s2 = (s2 + s1)     % ADLER_BASE;
	}
	return (s2 << 16) + s1;
}

/*
 * To not affect performance the simplest checksum algoritm will be used by default.
 *
 * The simplest checksum algorithm is the so-called longitudinal parity check, which breaks the data into "words"
 * with a fixed number n of bits, and then computes the exclusive or of all those words. The result is appended to
 * the message as an extra word. To check the integrity of a message, the receiver computes the exclusive or
 * of all its words, including the checksum; if the result is not a word with n zeros, the receiver knows that
 * a transmission error occurred.
 *
 * With this checksum, any transmission error that flips a single bit of the message, or an odd number of bits,
 * will be detected as an incorrect checksum. However, an error that affects two bits will not be detected if
 * those bits lie at the same position in two distinct words. If the affected bits are independently chosen at random,
 * the probability of a two-bit error being undetected is 1/n.
 * Also, this cannot detect bytes being out-of-order.
 *
 * NOTE: This is not an actual CRC.  But it is faster than any CRC algorithm.
 */
static pg_crc32
FileRep_CalculateParity(unsigned char *buf, int len)
{
	uint32 parity = 0;
	int n;
	unsigned long long temp = 0;
	int words = len / 8;
	int remaining = len % 8;

	/* Get 64-bits at a time, xor into temp */
	for (n = 0; n < words; n++)
	{
		unsigned long long w;

		/*
		 * Use memcpy because 'buf' might not be aligned. (Even on x86, which
		 * does not usually require alignment for memory accesses, the
		 * compiler might decide to optimize this with MMX instructions -
		 * which do require 16-bit alignment. We've seen this happen with gcc
		 * -O3 with some compiler versions. OTOH, as long as the target
		 * architecture doesn't require alignment, this will be optimized
		 * away and not cost anything.)
		 */
		memcpy(&w, &buf[n * 8], 8);
		temp ^= w;
	}

	/* Combine each of the 8 bytes from temp into parity, so we get the same answer as byte-by-byte loop */
	/*
	 * NOTE:  A single-byte parity can't tell us if we received the correct bytes but in the wrong order.
	 * Nor can it tell the case of two bytes being wrong, but the same bit in each byte.
	 *
	 * Rather than combine this into a single byte, we could just combine the two 32-bit halfs of the 64-bit
	 * value (via XOR), and then we would preserve far more information at no extra CPU cost.
	 *
	 * But the result would not satisfy the condition mentioned above:  If you compute a new Parity of the
	 * original message plus 4 bytes of parity, the result would not always be 32-bits of zero (the bad
	 * case is when the message is not a multiple of 4 bytes in size).
	 *
	 * This isn't really a problem if we only compare the Parity of the message with a previously computed parity.
	 * This really needs to have a code review.
	 *
	 * Of course, we could always do it the better way if the message is an even number of 32-bit words.
	 */
#if 0
	/* Combine each of the 8 bytes from temp into parity, so we get the same answer as byte-by-byte loop */
	parity ^=  ((temp >> 24) & 0xFF) ^ ((temp >> 16) & 0xFF) ^ ((temp >> 8) & 0xFF) ^ (temp & 0xFF);
	temp = temp >> 32;
	parity ^=  ((temp >> 24) & 0xFF) ^ ((temp >> 16) & 0xFF) ^ ((temp >> 8) & 0xFF) ^ (temp & 0xFF);
#else
	/* Reduce 64 bits to 32 bits. Maybe it would be better to circular shift one of the 32-bit values by 1 bit */
	parity ^= (temp >> 32) ^ (temp & 0xFFFFFFFF);
#endif

	/* If buf wasn't an even number of 64-bit words, we need to pick up the remaining bytes */
	for (n = 0; n < remaining; n++)
		parity ^= (buf + 8*words)[n];

	return parity;
}

/*
 * To not affect performance simple checksum algoritm is used by default.
 *
 * CRC Adler32 can be used by setting GUC 'filerep_crc_on = true'
 */
void
FileRep_CalculateCrc(
					 char*		data,
					 uint32		length,
					 pg_crc32	*crc)
{
	pg_crc32 _crc = 0;

	if (Debug_filerep_crc_on)
	{
		/* Compute Adler 32 */
		_crc = FileRep_CalculateAdler32((unsigned char *) data, length);
	}
	else
	{
		/* compute parity byte */
		_crc = FileRep_CalculateParity((unsigned char *) data, length);
	}

	*crc = _crc;
}

bool
FileRep_IsIpcSleep(FileRepOperation_e fileRepOperation)
{
	switch (fileRepOperation) {

		case FileRepOperationShutdown:
		case FileRepOperationHeartBeat:

			return TRUE;

		default:
			return FALSE;
	}
	return FALSE;
}

bool
FileRep_IsOperationSynchronous(FileRepOperation_e fileRepOperation)
{

	switch (fileRepOperation) {

		case FileRepOperationFlush:
		case FileRepOperationCreate:
		case FileRepOperationDrop:
		case FileRepOperationFlushAndClose:
		case FileRepOperationReconcileXLogEof:
		case FileRepOperationRename:
		case FileRepOperationShutdown:
		case FileRepOperationInSyncTransition:
		case FileRepOperationHeartBeat:
		case FileRepOperationCreateAndOpen:
		case FileRepOperationValidation:
		case FileRepOperationVerifySlruDirectoryChecksum:
			return TRUE;

		case FileRepOperationOpen:
		case FileRepOperationClose:
		case FileRepOperationTruncate:
		case FileRepOperationWrite:
		case FileRepOperationDropFilesFromDir:
		case FileRepOperationDropTemporaryFiles:
		case FileRepOperationStartSlruChecksum:
			return FALSE;

		case FileRepOperationNotSpecified:
			Assert(0);
			break;

        default:
            Assert(0);
            break;
	}
	return FALSE;
}

XLogRecPtr
FileRep_GetXLogRecPtrUndefined(void)
{
	XLogRecPtr r;

	r.xlogid = 0;
	r.xrecoff = 0;

	return r;
}

/*
 *
 */
FileRepIdentifier_u
FileRep_GetRelationIdentifier(
				char			*mirrorFilespaceLocation,
				RelFileNode		relFileNode,
				int32			segmentFileNum)
{
	FileRepIdentifier_u identifier;

	identifier.fileRepRelationIdentifier.relFileNode.spcNode = relFileNode.spcNode;
	identifier.fileRepRelationIdentifier.relFileNode.dbNode = relFileNode.dbNode;
	identifier.fileRepRelationIdentifier.relFileNode.relNode = relFileNode.relNode;

	identifier.fileRepRelationIdentifier.segmentFileNum = segmentFileNum;

	if (mirrorFilespaceLocation == NULL)
		sprintf(identifier.fileRepRelationIdentifier.mirrorFilespaceLocation,
				"%s",
				"");
	else
		sprintf(identifier.fileRepRelationIdentifier.mirrorFilespaceLocation,
				"%s",
				mirrorFilespaceLocation);

	return identifier;
}

FileRepIdentifier_u
FileRep_GetDirFilespaceIdentifier(
						 char				*mirrorFilespaceLocation)
{

	FileRepIdentifier_u identifier;

	identifier.fileRepDirIdentifier.fileRepDirType = FileRepDirFilespace;

	identifier.fileRepDirIdentifier.dbDirNode.tablespace = 0;
	identifier.fileRepDirIdentifier.dbDirNode.database = 0;

	if (mirrorFilespaceLocation == NULL)
		sprintf(identifier.fileRepDirIdentifier.mirrorFilespaceLocation,
				"%s",
				"");
	else
		sprintf(identifier.fileRepDirIdentifier.mirrorFilespaceLocation,
				"%s",
				mirrorFilespaceLocation);

	return identifier;
}

FileRepIdentifier_u
FileRep_GetDirTablespaceIdentifier(
						 char		*mirrorFilespaceLocation,
						 Oid		tablespace)
{

	FileRepIdentifier_u identifier;

	identifier.fileRepDirIdentifier.fileRepDirType = FileRepDirTablespace;

	identifier.fileRepDirIdentifier.dbDirNode.tablespace = tablespace;
	identifier.fileRepDirIdentifier.dbDirNode.database = 0;

	if (mirrorFilespaceLocation == NULL)
		sprintf(identifier.fileRepDirIdentifier.mirrorFilespaceLocation,
				"%s",
				"");
	else
		sprintf(identifier.fileRepDirIdentifier.mirrorFilespaceLocation,
				"%s",
				mirrorFilespaceLocation);

	return identifier;
}

FileRepIdentifier_u
FileRep_GetDirDatabaseIdentifier(
						 char				*mirrorFilespaceLocation,
						 DbDirNode			dbDirNode)
{

	FileRepIdentifier_u identifier;

	identifier.fileRepDirIdentifier.fileRepDirType = FileRepDirDatabase;

	identifier.fileRepDirIdentifier.dbDirNode.tablespace = dbDirNode.tablespace;
	identifier.fileRepDirIdentifier.dbDirNode.database = dbDirNode.database;

	if (mirrorFilespaceLocation == NULL)
		sprintf(identifier.fileRepDirIdentifier.mirrorFilespaceLocation,
				"%s",
				"");
	else
		sprintf(identifier.fileRepDirIdentifier.mirrorFilespaceLocation,
				"%s",
				mirrorFilespaceLocation);

	return identifier;
}

FileRepIdentifier_u
FileRep_GetFlatFileIdentifier(
				char*	subDirectory,
				char*	simpleFileName)
{

	FileRepIdentifier_u identifier;

	strncpy(identifier.fileRepFlatFileIdentifier.directorySimpleName,
			subDirectory,
			MAXPGPATH);

	strncpy(identifier.fileRepFlatFileIdentifier.fileSimpleName,
			simpleFileName,
			MAXPGPATH);

	identifier.fileRepFlatFileIdentifier.blockId = FILEREPFLATFILEID_WHOLE_FILE;

	return identifier;
}

FileRepIdentifier_u
FileRep_GetUnknownIdentifier(
							  char	*unknownId)
{

	FileRepIdentifier_u identifier;

	Assert(strlen(unknownId) + 1 <= sizeof(identifier.fileRepUnknownIdentifier.unknownId));

	/*
	 * +1 for the data copied to ensure we copy trailing 0 for when the length is equal to
	 *     FILEREP_UNKNOWN_IDENTIFIER_LEN
	 */
	strlcpy(identifier.fileRepUnknownIdentifier.unknownId,
			unknownId,
			FILEREP_UNKNOWN_IDENTIFIER_LEN + 1);

	return identifier;
}


/*
 * $PGDATA/filespace/segment/tablespace_oid/database_oid/relfilenode_oid
 */
FileName
FileRep_GetFileName(
					  FileRepIdentifier_u	 fileRepIdentifier,
					  FileRepRelationType_e	fileRepRelationType)
{
	FileName fileName = NULL;

	switch (fileRepRelationType) {
		case FileRepRelationTypeFlatFile:

			fileName = (char*) palloc(
				strlen(fileRepIdentifier.fileRepFlatFileIdentifier.directorySimpleName) +
				strlen(fileRepIdentifier.fileRepFlatFileIdentifier.fileSimpleName) + 2);

			sprintf(fileName, "%s/%s",
					fileRepIdentifier.fileRepFlatFileIdentifier.directorySimpleName,
					fileRepIdentifier.fileRepFlatFileIdentifier.fileSimpleName);


			break;

		case FileRepRelationTypeBulk:
		case FileRepRelationTypeBufferPool:
		case FileRepRelationTypeAppendOnly:

			fileName = (char*) palloc(MAXPGPATH+1+20); /* + 20 is to add room for . and the segment file num */

			FormRelationPath(
							 fileName,
							 fileRepIdentifier.fileRepRelationIdentifier.mirrorFilespaceLocation,
							 fileRepIdentifier.fileRepRelationIdentifier.relFileNode);

			if (fileRepIdentifier.fileRepRelationIdentifier.segmentFileNum != 0)
			{
				sprintf(fileName, "%s.%u",
						fileName,
						fileRepIdentifier.fileRepRelationIdentifier.segmentFileNum);
			}

			break;

		case FileRepRelationTypeDir:

			fileName = (char*) palloc(MAXPGPATH+1);

			switch (fileRepIdentifier.fileRepDirIdentifier.fileRepDirType)
			{
				case FileRepDirFilespace:
					sprintf(fileName, "%s",
							fileRepIdentifier.fileRepDirIdentifier.mirrorFilespaceLocation);
					break;

				case FileRepDirTablespace:
					if (strcmp(fileRepIdentifier.fileRepDirIdentifier.mirrorFilespaceLocation,
							   "") == 0)
						FormTablespacePath(
										   fileName,
										   NULL,
										   fileRepIdentifier.fileRepDirIdentifier.dbDirNode.tablespace);
					else
						FormTablespacePath(
										   fileName,
										   fileRepIdentifier.fileRepDirIdentifier.mirrorFilespaceLocation,
										   fileRepIdentifier.fileRepDirIdentifier.dbDirNode.tablespace);

					break;

				case FileRepDirDatabase:
					if (strcmp(fileRepIdentifier.fileRepDirIdentifier.mirrorFilespaceLocation,
							   "") == 0)
						FormDatabasePath(
										 fileName,
										 NULL,
										 fileRepIdentifier.fileRepDirIdentifier.dbDirNode.tablespace,
										 fileRepIdentifier.fileRepDirIdentifier.dbDirNode.database);
					else
						FormDatabasePath(
										 fileName,
										 fileRepIdentifier.fileRepDirIdentifier.mirrorFilespaceLocation,
										 fileRepIdentifier.fileRepDirIdentifier.dbDirNode.tablespace,
										 fileRepIdentifier.fileRepDirIdentifier.dbDirNode.database);
					break;

				default:
					Assert(0);
					break;
			}
			break;

		case FileRepRelationTypeUnknown:

			fileName = (char*) palloc(FILEREP_UNKNOWN_IDENTIFIER_LEN + 1);
			sprintf(fileName, "%s",
					fileRepIdentifier.fileRepUnknownIdentifier.unknownId);
			break;

		case FileRepRelationTypeNotSpecified:
			break;

		default:
			Assert(0);
			break;
	}

	return fileName;
}

void
FileRep_GetRelationPath(
					char		*relationPath,
					RelFileNode relFileNode,
					uint32		segmentFileNum) { char *primaryFilespaceLocation;
	char *mirrorFilespaceLocation;

	PersistentTablespace_GetPrimaryAndMirrorFilespaces(
													   relFileNode.spcNode,
													   &primaryFilespaceLocation,
													   &mirrorFilespaceLocation);

	FormRelationPath(
					 relationPath,
					 mirrorFilespaceLocation,
					 relFileNode);

	if (segmentFileNum != 0)
	{
		sprintf(relationPath, "%s.%u",
				relationPath,
				segmentFileNum);
	}

	if (primaryFilespaceLocation != NULL)
		pfree(primaryFilespaceLocation);

	if (mirrorFilespaceLocation != NULL)
		pfree(mirrorFilespaceLocation);
}

/*
 * Postmaster is responsible to start, monitor and exit FileRep process.
 * FileRep process is required only if segment has mirror configured.
 *
 * FileRep process is started on Primary and its Mirror by postmaster at
 *		*) gpstart
 *		*) gprecoverseg
 *		*) gpaddmirrors (if utility is not deprecated)
 *
 * FileRep process is exited on Primary and its Mirror by postmaster at
 *		*) gpfailover
 *		*) gpstop
 *
 *
 * FileRep process state transitions
 *
 *					   gpstart
 *					   gprecoverseg
 *					   gpaddmirrors
 *							|
 *						   \|/
 *					-------------------
 *					| INITIALIZATION  |
 *					-------------------
 *							|
 *						   \|/
 *					-------------------
 *					|	  READY		  |
 *					-------------------
 *				gpstop|	         	|
 *          gpfailover|				|
 *					 \|/	       \|/
 *         -------------  gpstop   ---------------
 *         |  SHUTDOWN | <---------|   FAULT     |
 *         ------------- gpfailover---------------
 *                |
 *	  		     \|/
 *		  ----------------
 *		  |	    DOWN	 |
 *		  ----------------
 *
 *
 * START Postmaster forks FileRep process. Input parameters for FileRep
 *		process are stored in shared memory.  Postmaster-backend process
 *		writes input parameters. FileRep and Postmaster processes read them
 *		(never write).
 *
 *		After FileRep process completes Initialization phase it
 *      signal SIGUSR1 (reason PMSIGNAL_RECOVERY_CONSISTENT)
 *      to postmaster. Postmaster (if Primary Role)
 *		proceeds with File System object and Wal Recovery.
 *
 *		In addition postmaster
 *			*) initialize and populate shared memory for FileSpaces
 *			*) initialize shared memory for FileRep mirror messages
 *			*) initialize shared memory for FileRep Ack mirror messages
 *			*) FS_OBJECTS is TBD
 *			*) dedicate shared memory for maintaining FileRepState_e
 *			   and postmaster PID.
 *
 * MONITOR
 *		Postmaster monitors FileRep process.
 *
 *			If the FileRep process exits unexpectedly, the postmaster
 *			treats that condition as Fault. Fault is reported to
 *			Faulty coordinator, that determines recovery procedure.
 *
 *			If the FileRep process detects an error then it sends
 *			signal SIGUSR1 (reason PMSIGNAL_FAULT) to its postmaster
 *			and waits in "Fault" state.
 *			The postmaster
 *			treats that condition as Fault. Fault is reported to
 *			Faulty coordinator, that determines recovery procedure.
 *
 * EXIT
 *		Postmaster on a segment behaves differently for graceful
 *		and emergency process termination.
 *			a) Graceful termination
 *					1) Postmaster signals termination to all processes
 *					   except FileRep process
 *					2) After Postmaster receives that all processes
 *					   exit then it signals FileRep process
 *					3) After Postmaster receives that FileRep process
 *					   exit then it cleans shared memory
 *
 *			b) Emergency termination
 *					1) Postmaster signals termination to all processes
 *					2) After Postmaster receives that all processes
 *					   exit then it cleans shared memory
 *
 *		Postmaster exits the FileRep process by graceful or emergency
 *		termination.
 *			a) Graceful termination is performed by signaling SIGUSR2
 *				to FileRep process on primary and its mirror.
 *				FileRep process in Primary Role
 *					*) ensure all data are flushed
 *						-) no message is located in FileRep shared memory
 *						-) no message is located in FileRep Ack shared memory
 *					*) inform Mirror about Shutdown
 *						-) Mirror close ACK connection to primary
 *						-) Mirror will not report fault, but wait
 *						   to get shutdown from mirror postmaster
 *						-) flush to persistent storage (file) information
 *					       for fast recovery (failover optimization)
 *						-) flush to persistent storage (file) information
 *						   about Replication Role and Replication
 *						   State (for integrity check)
 *					*) close connection to mirror
 *					*) clean up (free memory, ...)
 *					*) exit with sending signal SIGCHLD (status=OK)
 *					   to postmaster
 *
 *				FileRep process in Mirror Role
 *					*) ensure Primary did clean shutdown
 *					*) clean up (free memory, ...)
 *					*) exit with sending signal SIGCHLD (status=OK)
 *					   to postmaster
 *
 *			b) Emergency termination is performed by signaling SIGQUIT
 *			   to FileRep process.
 *
 *				FileRep process in Primary and Mirror Role aborts
 *				work in progress, close connection, clean up
 *				and exit with sending signal SIGCHLD
 *				(status=ABORT) to postmaster.
 *
 */
void FileRep_Main(void)
{
	SegmentState_e	segmentState = SegmentStateNotInitialized;

	int		status = STATUS_OK;

	char title[100];

	sigjmp_buf		local_sigjmp_buf;

	/*
	 * Enum in cdpfilerep.h should match TypetoString declaration here in cdpfilerep.c
	 */
    COMPILE_ASSERT(ARRAY_SIZE(FileRepProcessTypeToString) == FileRepProcessType__EnumerationCount);
    COMPILE_ASSERT(ARRAY_SIZE(FileRepRoleToString) == FileRepRole_e_Count);
    COMPILE_ASSERT(ARRAY_SIZE(SegmentStateToString) == SegmentState__EnumerationCount);
    COMPILE_ASSERT(ARRAY_SIZE(DataStateToString) == DataState__EnumerationCount);
    COMPILE_ASSERT(ARRAY_SIZE(FileRepStateToString) == FileRepState__EnumerationCount);
    COMPILE_ASSERT(ARRAY_SIZE(FileRepOperationToString) == FileRepOperation__EnumerationCount);
    COMPILE_ASSERT(ARRAY_SIZE(FileRepRelationTypeToString) == FileRepRelationType__EnumerationCount);
    COMPILE_ASSERT(ARRAY_SIZE(FileRepAckStateToString) == FileRepAckState__EnumerationCount);
    COMPILE_ASSERT(ARRAY_SIZE(FileRepConsumerProcIndexToString) == FileRepMessageType__EnumerationCount);
	COMPILE_ASSERT(ARRAY_SIZE(FileRepStatusToString) == FileRepStatus__EnumerationCount);
	Insist(IsUnderPostmaster == TRUE);

	fileRepProcessType = FileRepProcessTypeMain;

	FileRep_ConfigureSignals();

	main_tid = pthread_self();

	/*
	 * If an exception is encountered, processing resumes here.
	 *
	 * See notes in postgres.c about the design of this coding.
	 */
	if (sigsetjmp(local_sigjmp_buf, 1) != 0)
	{
		/* Prevents interrupts while cleaning up */
		HOLD_INTERRUPTS();

		/* Report the error to the server log */
		EmitErrorReport();

		LWLockReleaseAll();

		/*
		 * We can now go away.	Note that because we'll call InitProcess, a
		 * callback will be registered to do ProcKill, which will clean up
		 * necessary state.
		 */
		proc_exit(0);
	}

	/* We can now handle ereport(ERROR) */
	PG_exception_stack = &local_sigjmp_buf;

	PG_SETMASK(&UnBlockSig);

	/* Create the memory context where cross-transaction state is stored */
	fileRepMemoryContext = AllocSetContextCreate(TopMemoryContext,
										  "filerep memory context",
										  ALLOCSET_DEFAULT_MINSIZE,
										  ALLOCSET_DEFAULT_INITSIZE,
										  ALLOCSET_DEFAULT_MAXSIZE);

	MemoryContextSwitchTo(fileRepMemoryContext);

	FileRep_SetState(FileRepStateInitialization);

	FileRep_SetFileRepRetry();

	FileRepIpc_ShmemReInit();
	FileRep_ShmemReInit();
	FileRepAck_ShmemReInit();
	FileRepLog_ShmemReInit();

	MemoryContext oldContext = MemoryContextSwitchTo(TopMemoryContext);
	primaryMirrorGetFilerepArguments(&fileRepRole, &segmentState, &dataState, &fileRepPrimaryHostAddress,
	    &fileRepPrimaryPort, &fileRepMirrorHostAddress, &fileRepMirrorPort, &fileRepFullResync );
    MemoryContextSwitchTo(oldContext);

	if (fileRepRole == FileRepNoRoleConfigured) {
		 ereport(WARNING,
				 (errmsg("mirror failure, "
						 "could not retrieve segment configuration, "
						 "failover requested"),
				  errhint("run gprecoverseg to re-establish mirror connectivity")));

		 FileRep_SetState(FileRepStateFault);
		goto shutdown;
	}

	/*
	 * Change Tracking shared memory is not re-initialized during transition to resync since
	 * Change Tracking buffers are flushed during transition to resync under MirroredLock.
	 */
	if (segmentState != SegmentStateInResyncTransition)
	{
		ChangeTrackingShmemReset();
	}

	snprintf(title, sizeof(title), "%s",
			 fileRepRole == FileRepPrimaryRole ?
			 	"primary process" : "mirror process");
	init_ps_display(title, "", "", "");

	{
		char	tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];

		snprintf(tmpBuf, sizeof(tmpBuf), "mirror transition, primary address(port) '%s(%d)' mirror address(port) '%s(%d)' ",
				 fileRepPrimaryHostAddress,
				 fileRepPrimaryPort,
				 fileRepMirrorHostAddress,
				 fileRepMirrorPort);

		FileRep_InsertConfigLogEntry(tmpBuf);
	}

	ereport(LOG,
			(errmsg("mirror transition, "
					"primary address(port) '%s(%d)' mirror address(port) '%s(%d)' ",
			 fileRepPrimaryHostAddress,
			 fileRepPrimaryPort,
			 fileRepMirrorHostAddress,
			 fileRepMirrorPort),
			 FileRep_errcontext()));

	if (dataState == DataStateInChangeTracking)
	{
		if (segmentState == SegmentStateFault)
		{
			FileRep_SetPostmasterReset();
		}

		if (FileRep_StartChildProcess(FileRepProcessTypePrimaryRecovery) < 0) {
			status = STATUS_ERROR;
			FileRep_SetPostmasterReset();
		}

		/* enter shutdown state -- wait for the postmaster to stop us */
		goto shutdown;
	}

	Insist(dataState == DataStateInSync || dataState == DataStateInResync);

	/*
	 * Initialize subprocess list just before sawning them.
	 */
	memcpy(FileRepSubProcList,
			FileRepSubProcListInitial, sizeof(FileRepSubProcListInitial));

	switch (fileRepRole) {
		case FileRepPrimaryRole:

			FileRep_ShmemInitArray(
								   fileRepAckShmemArray,
								   FILEREP_ACKSHMEM_MESSAGE_SLOTS_PRIMARY,
								   file_rep_ack_buffer_size,
								   false);

			if (FileRep_StartChildProcess(FileRepProcessTypePrimaryReceiverAck) < 0) {
				status = STATUS_ERROR;
				break;
			}

			if (FileRep_StartChildProcess(FileRepProcessTypePrimarySender) < 0) {
				status = STATUS_ERROR;
				break;
			}

			if (FileRep_StartChildProcess(FileRepProcessTypePrimaryConsumerAck) < 0) {
				status = STATUS_ERROR;
				break;
			}

			if (FileRep_StartChildProcess(FileRepProcessTypePrimaryRecovery) < 0) {
				status = STATUS_ERROR;
				break;
			}

			if (dataState == DataStateInResync)
			{
				if (FileRep_StartChildProcess(FileRepProcessTypeResyncManager) < 0) {
					status = STATUS_ERROR;
					break;
				}
				if (FileRep_StartChildProcess(FileRepProcessTypeResyncWorker1) < 0) {
					status = STATUS_ERROR;
					break;
				}
				if (FileRep_StartChildProcess(FileRepProcessTypeResyncWorker2) < 0) {
					status = STATUS_ERROR;
					break;
				}
				if (FileRep_StartChildProcess(FileRepProcessTypeResyncWorker3) < 0) {
					status = STATUS_ERROR;
					break;
				}
				if (FileRep_StartChildProcess(FileRepProcessTypeResyncWorker4) < 0) {
					status = STATUS_ERROR;
					break;
				}

			}


			break;

		case FileRepMirrorRole:

			FileRep_ShmemInitArray(
								   fileRepShmemArray,
								   FILEREP_SHMEM_MESSAGE_SLOTS_MIRROR,
								   file_rep_buffer_size,
								   true);

			if (FileRep_StartChildProcess(FileRepProcessTypeMirrorReceiver) < 0) {
				status = STATUS_ERROR;
				break;
			}

			if (FileRep_StartChildProcess(FileRepProcessTypeMirrorConsumer) < 0) {
				status = STATUS_ERROR;
				break;
			}

			if (FileRep_StartChildProcess(FileRepProcessTypeMirrorConsumerWriter) < 0) {
				status = STATUS_ERROR;
				break;
			}

			if (FileRep_StartChildProcess(FileRepProcessTypeMirrorConsumerAppendOnly1) < 0) {
				status = STATUS_ERROR;
				break;
			}

			if (FileRep_StartChildProcess(FileRepProcessTypeMirrorSenderAck) < 0) {
				status = STATUS_ERROR;
				break;
			}

			break;
		case FileRepNoRoleConfigured:
		case FileRepRoleNotInitialized:
		default:
			Assert(0);
			break;

	}

	if (status != STATUS_OK) {
		FileRep_SetState(FileRepStateFault);
		goto shutdown;
	}


shutdown:

	while (FileRep_ProcessSignals() == false) {

		if ( FileRep_GetState() == FileRepStateShutdownBackends &&
		     ! FileRep_IsAnyBackendChildAlive())
        {
            /* tell postmaster that filerep backends have been shutdown */
            SendPostmasterSignal(PMSIGNAL_PRIMARY_MIRROR_ALL_BACKENDS_SHUTDOWN);
        }

		FileRep_CheckFlushLogs(fileRepConfigLogShmem, TRUE /* isConfigLog */);

		FileRep_CheckFlushLogs(fileRepLogShmem, FALSE /* isConfigLog */);

		/*
		 * check and process any signals received
		 * The routine returns TRUE if the received signal requests
		 * process shutdown.
		 */

		pg_usleep(1000000L); // 1sec
	}

    /* wait for all children to terminate */
    while ( FileRep_IsAnyChildAlive())
    {
        FileRep_ProcessSignals();
        if ( ! FileRep_IsAnyChildAlive())
            break;
			pg_usleep(1000000L); // 1sec
    }

	FileRep_FlushLogActiveSlot();

	if (exitWithImmediateShutdown)
	{
		proc_exit(2);
	}
	else
	{
		proc_exit(0);
	}
} // FileRep_Main()


/*
 * Reset (unlock) all filerep spin locks.
 */
void FileRep_resetSpinLocks(void)
{
	if (fileRepLogShmem != NULL)
	{
		SpinLockInit(&fileRepLogShmem->logLock);
	}
	if (fileRepConfigLogShmem != NULL)
	{
		SpinLockInit(&fileRepConfigLogShmem->logLock);
	}
}
