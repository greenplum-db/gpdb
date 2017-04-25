/*
 *  cdbfilerep.h
 *
 *
 *  Copyright 2009-2010, Greenplum Inc. All rights reserved.
 *
 */

#ifndef CDBFILEREP_H
#define CDBFILEREP_H

#include <signal.h>

#include "utils/pg_crc.h"
#include "storage/relfilenode.h"
#include "access/slru.h"
#include "access/xlogdefs.h"
#include "postmaster/primary_mirror_mode.h"
#include "storage/fd.h"
#include "storage/dbdirnode.h"
#include "storage/lwlock.h"
#include "storage/pg_sema.h"
#include "gpmon/gpmon.h"
#include "utils/timestamp.h"



	/* ATTENTION: Code has to be changed if FILEREP_MAX_LISTEN > 1 */
#define FILEREP_MAX_LISTEN	1
	/* Max number of listener */
#define FILEREP_MESSAGEBODY_LEN 64 * 1024
	/* AppendOnly writes 64k and BufferPool writes 32k */

#define FILEREP_MAX_OPEN_FILES 128 // 131072   // 128k

#define FILEREP_MAX_IPC_ARRAY	5
#define FILEREP_MAX_SEMAPHORES	2 * FILEREP_MAX_IPC_ARRAY

#define FILEREP_MAX_RESYNC_FILES 128  //8k

#define FILEREP_OFFSET_UNDEFINED -1

#define FILEREP_UNDEFINED 0xFFFFFFFF

#define FILEREP_UNKNOWN_IDENTIFIER_LEN	16

#define FILEREP_LOG_MAX_SIZE 1024 * 1024 * 100 //100M

#define FILEREP_MAX_LOG_DESCRIPTION_LEN 128

#define FileRepConfigLogPath(path) \
		snprintf(path, MAXPGPATH, CHANGETRACKINGDIR "/FILEREP_CONFIG_LOG")

#define FileRepLogPath(path) \
	snprintf(path, MAXPGPATH, CHANGETRACKINGDIR "/FILEREP_LOG")

#define FILEREPFLATFILEID_WHOLE_FILE -1

/*
 * 'file_rep_retry == 0' indicates retry forever
 */
#define FileRep_IsRetry(retry) \
		(retry < file_rep_retry || file_rep_retry == 0)


#define FileRep_IncrementRetry(retry)										\
			if ((file_rep_retry != 0) ||								\
				(file_rep_retry == 0 && retry < FILEREP_UNDEFINED))		\
				retry++;

/* GUC parameters */
extern int file_rep_message_body_length;
extern int file_rep_buffer_size;
extern int file_rep_ack_buffer_size;
extern int file_rep_retry;
extern int file_rep_min_data_before_flush;
extern int file_rep_socket_timeout;
extern int file_rep_mirror_consumer_process_count;

extern FileRepRole_e		fileRepRole;
extern SegmentState_e		segmentState;
extern DataState_e			dataState;

/* The IPv4 address of the primary segment. */
extern char *fileRepPrimaryHostAddress;

/* The communication port of the primary. Required for listener ACK. */
extern int fileRepPrimaryPort;

/* The IPv4 address of the mirror. */
extern char *fileRepMirrorHostAddress;

/* The communication port of the mirror. Required for listener. */
extern int fileRepMirrorPort;

/* whether a full resync has been requested on startup */
extern bool fileRepFullResync;

extern const char*	FileRepProcessTypeToString[];
extern const char*	FileRepRoleToString[];
extern const char*	SegmentStateToString[];
extern const char*	DataStateToString[];
extern const char*  FileRepStateToString[];
extern const char*	FileRepOperationToString[];
extern const char*	FileRepRelationTypeToString[];
extern const char*	FileRepAckStateToString[];
extern const char*	FileRepStatusToString[];
extern const char*  FileRepOperationVerificationTypeToString[];

/****************************************************************
 * FILEREP SHARED MEMORY
 ****************************************************************/
/*
 * The structure describes position for next message to be inserted
 * and next message to be consumed. The same structure is used on
 * primary and mirror.
 *
 *  beginPosition
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

typedef struct FileRepShmem_s {

	int		ipcArrayIndex;

	char*	positionBegin;
	/*
	 * Begin position for buffering messages.
	 * It is initialized when shared memory is initalized.
	 * The value does not change after.
	 */

	char*	positionEnd;
	/*
	 * End position for buffering messages.
	 * It is initialized when shared memory is initalized.
	 * The value does not change after.
	 */

	char*	positionInsert;
	/*
	 * Identifies the insert position in shared memory for next message.
	 * It is protected by FileRepShmemLock.
	 */

	char*	positionConsume;
	/*
	 * Identifies the position in shared memory for next message to be
	 * mirrored. It is protected by FileRepShmemLock.
	 */

	char* positionWraparound;
	/*
	 * Identifies the end of last message in shared memory.
	 * It is protected by FileRepShmemLock
	 * The value is updated at wraparound when message is inserted
	 * at startPosition in shared memory.
	 * It is used by consumer thread in order to detect wraparound.
	 */

	uint32	insertCount;
	/* counter of inserted messages. It is used for testing code correctness. */

	uint32	consumeCount;
	/* counter of consumed messages. It is used for testing code correctness. */

	volatile sig_atomic_t state;

} FileRepShmem_s;

/*
 * The structure describes the state of the FileRep message
 * in shared memory.
 */
typedef enum FileRepShmemMessageState_e {

	FileRepShmemMessageStateInitialized=0,
		/* Memory is set to 0 (memset(). */

	FileRepShmemMessageStateReserved,
		/* Memory is reserved for next message to be inserted. */

	FileRepShmemMessageStateReady
		/* Message is inserted and ready to be consumed. */

} FileRepShmemMessageState_e;

/*
 * The structure describes FileRep Message related information
 * that is required for consuming FileRep message (removing message
 * from shared memory and send it over the network to mirror).
 * That information is kept only in shared memory.
 */
typedef struct FileRepShmemMessageDescr_s {

	bool			messageSync;
	/*
	 * Synchronous FileRep Message.
	 * Client flushes all data from buffers when
	 * synchronous message is sent.
	 */

	uint32			messageLength;
	/* FileRep Message length (FileRepMessage_s) */

	volatile sig_atomic_t	messageState;
	/*
	 * FileRep Message state.
	 *		a) Reserved state (reserve memory for next message insert)
	 *		b) Ready state (calculate Crc and insert message)
	 *		   Message is ready to be consumed.
	 */

} FileRepShmemMessageDescr_s;

/*
 *
 */
typedef struct FileRepIpcShmem_s
{
	PGSemaphoreData	semP;

	PGSemaphoreData semC;

	int		refCountSemP;

	int		refCountSemC;

} FileRepIpcShmem_s;

extern FileRepIpcShmem_s	*fileRepIpcArray[FILEREP_MAX_IPC_ARRAY];

typedef enum FileRep_IndexIpcArray_e
{
	IndexIpcArrayAckShmem = 0,
	IndexIpcArrayShmem,
	IndexIpcArrayAckHashShmem,
	IndexIpcArrayVerify

} FileRep_IndexIpcArray_e;

/*
 * Return the required number of semaphores for this module.
 */
extern int FileRepSemas(void);

extern Size FileRepIpc_ShmemSize(void);

extern void FileRepIpc_ShmemInit(void);

/*
 * Block
 */
extern void FileRep_IpcWait(PGSemaphoreData sem, int *refCount, LWLockId lockId);

/*
 * Unblock
 */
extern void FileRep_IpcSignal(PGSemaphoreData sem, int* refCount);

/*
 * Unblock all waiting semaphores
 */
extern void FileRep_IpcSignalAll(void);

extern void FileRep_SignalChildren(int signal, bool backendsOnly);

/*
 * Return the required shared-memory size for subprocess list.
 */
extern Size FileRep_SubProcShmemSize(void);

/*
 * Initialize the shared-memory for subprocess list.
 */
extern void FileRep_SubProcShmemInit(void);

/*
 * Return the required shared-memory size for this module.
 */
extern Size FileRep_ShmemSize(void);

/*
 * Initialize the shared-memory for this module.
 */
extern void FileRep_ShmemInit(void);

/*
 * Return the required shared-memory size for this module.
 */
extern Size FileRepAck_ShmemSize(void);

/*
 * Initialize the shared-memory for this module.
 */
extern void FileRepAck_ShmemInit(void);



/****************************************************************
 * FILEREP PROCESS
 ****************************************************************/
/*
 * PRIMARY
 *			*) FileRepShmem has 1 slot. All filerep messages are inserted into that slot.
 *			*) FileRepAckShmem has 2 slots. Indexing into slot is done via #define
 *			   FILEREP_ACKSHMEM_MESSAGE_SLOT_PRIMARY_ACK and
 *			   FILEREP_ACKSHMEM_MESSAGE_SLOT_PRIMARY_VERIFY
 *					1) Slot 0 is dedicated for all ACK filerep related messages.
 *					   These messages has no data body.
 *					2) Slot 1 is dedicated for all ACK online verification related messages.
 *					   These messages may have data body.
 *
 *	MIRROR
 *			*) FileRepShmem has 4 slots. Messages are inserted into slots based on relation type.
 *			   An exception is online verification that has dedicated slot.
 *			   Indexing into slot is done via msgType.
 *					1) Slot 0 is dedicated to flat files (i.e. transaction logs) related messages
 *					2) Slot 1 is dedicated to online verification
 *					3) Slot 2 is dedicated to append only files related messages
 *					4) Slot 3 is dedicated to heap files related messages
 *			*)	FileRepAckShmem has 1 slot. All ack filerep messages are inserted into that slot.
 */
#define FILEREP_ACKSHMEM_MESSAGE_SLOTS_PRIMARY	2
#define FILEREP_ACKSHMEM_MESSAGE_SLOTS_MIRROR	1

#define FILEREP_SHMEM_MESSAGE_SLOTS_PRIMARY 1
#define FILEREP_SHMEM_MESSAGE_SLOTS_MIRROR	4

#define FILEREP_SHMEM_MAX_SLOTS		Max(FILEREP_SHMEM_MESSAGE_SLOTS_PRIMARY, FILEREP_SHMEM_MESSAGE_SLOTS_MIRROR)
#define FILEREP_ACKSHMEM_MAX_SLOTS	Max(FILEREP_ACKSHMEM_MESSAGE_SLOTS_PRIMARY, FILEREP_ACKSHMEM_MESSAGE_SLOTS_MIRROR)

#define FILEREP_ACKSHMEM_MESSAGE_SLOT_PRIMARY_ACK		0 /* Slot #0 is dedicated for ACK */
#define FILEREP_ACKSHMEM_MESSAGE_SLOT_PRIMARY_VERIFY	1 /* Slot #1 is dedicated for online verification */

/*
I might have been tempted to use OutgoingMessageQueue and IncomingMessageQueue as main abstraction names
outgoing message queue would always have 1 slot and incoming would have mailboxes for incoming messages

*/
//JNM TODO replace fileRepProcIndex with FILEREP_OUTGOING_MESSAGE_QUEUE ?
#define FILEREP_OUTGOING_MESSAGE_QUEUE 0

#define FILEREP_VERIFY_MAX_REQUEST_TOKEN_LEN 63

extern FileRepShmem_s	*fileRepShmemArray[FILEREP_SHMEM_MAX_SLOTS];

extern FileRepShmem_s	*fileRepAckShmemArray[FILEREP_ACKSHMEM_MAX_SLOTS];

typedef enum FileRepState_e {
	FileRepStateNotInitialized=0,
	/* */

	FileRepStateInitialization,
	/*
	 * Start listener, establish connection and reconcile
	 * WAL to same EOF (if in-sync state).
	 * Establish Connection (if in resync data state)
	 */
	FileRepStateReady,
	/*
	 * Mirroring or resynchronization is in progress.
	 * dataState is InChangeTracking or InResync.
	 */

	FileRepStateFault,
	/*
	 * Fault is detected on primary or mirror.
	 * It transitions into Suspend state.
	 */
    FileRepStateShutdownBackends,
    /* Backend-like processes are going to exit (graceful termination)
     *
     * Note that handling of this in cdbfilerepservice is a little special:
     *  it is NEVER copied into the local state, but either ignored
     *  or converted to FileRepStateShutdown for local purposes.
     */

	FileRepStateShutdown,
	/* Process is going to exit (graceful termination). */

	FileRepState__EnumerationCount
} FileRepState_e;

/*
 * Responsibilities of this module!!!
 *
 * Mirror Message contains
 *		a) Header (describes operation and data that are mirrored)
 *		b) Crc of the header
 *		c) Body (contains data that are mirrored.). If mirror operation
 *         is different than 'write' then data are not mirrored
 *         and hence Body is empty.
 *
 *   --------------------------------------------------------------
 *  | Message       | Message     | Message Body                   |
 *  | Header        | Header CRC  |     configurable size          |
 *  |               |     4 bytes |     default is 32k bytes       |
 *   --------------------------------------------------------------
 *
 */

/*
 *
 */
typedef enum FileRepStatus_e {
	FileRepStatusSuccess=0,

	FileRepStatusNotDirectory,

	FileRepStatusNoSuchFileOrDirectory,

	FileRepStatusErrorReadingDirectory,

	FileRepStatusNotEmptyDirectory,

	FileRepStatusDirectoryExist,

	FileRepStatusNoSpace,

	FileRepStatusNoPermissions,

	FileRepStatusReadOnlyFileSystem,

	FileRepStatusMirrorLossOccurred,

	FileRepStatusMirrorError,

	/* checksum verification failed on the mirror */
	FileRepStatusSlruChecksumFailed,

	/* the number of values in this enumeration */
	FileRepStatus__EnumerationCount

} FileRepStatus_e;

typedef enum FileRepMessageHeaderVersion_e {
	FileRepMessageHeaderVersionOne=0,
} FileRepMessageHeaderVersion_e;

typedef enum FileRepConsumerProcIndex_e {
	FileRepMessageTypeXLog=0,

	FileRepMessageTypeAO01,

	FileRepMessageTypeWriter,

	FileRepMessageTypeShutdown,

	FileRepMessageTypeUndefined,


	/* the number of values in this enumeration */
	FileRepMessageType__EnumerationCount
} FileRepConsumerProcIndex_e;

extern FileRepConsumerProcIndex_e fileRepProcIndex;

/*
 * Describe relation types that are mirrored.
 */
typedef enum FileRepRelationType_e {
	FileRepRelationTypeFlatFile=0,
		/* Mirror Relation is flat file. */

	FileRepRelationTypeAppendOnly,
		/* Mirror Relation is Append Only file */

	FileRepRelationTypeBufferPool,
		/* Mirror Relation is Heap file. */

	FileRepRelationTypeBulk,
		/* Mirror Relation is bulk loaded file. */

	FileRepRelationTypeDir,
		/* Mirror Relation is generic directory create and drop. */

	FileRepRelationTypeUnknown,

	FileRepRelationTypeNotSpecified,

	/* the number of values in this enumeration */
	FileRepRelationType__EnumerationCount

} FileRepRelationType_e;

/*
 * Describe relation operations that are mirrored.
 */
typedef enum FileRepOperation_e {
	FileRepOperationOpen=0,
		/*	Open mirror relation or flat file. */

	FileRepOperationWrite,
		/* Write data to a mirror. */

	FileRepOperationFlush,
		/* Issue synchronous flush on a mirror. */

	FileRepOperationClose,
		/* Close a mirror relation or flat file. */

	FileRepOperationFlushAndClose,
		/* Issue synchronous flush and close a mirror relation */

	FileRepOperationTruncate,
	/* Issue truncate to a mirror relation */

	FileRepOperationCreate,
		/* Create a mirror relation, flat file, or database directory. */

	FileRepOperationDrop,
		/* Drop mirror relation, flat file, or database directory. */

	FileRepOperationReconcileXLogEof,
		/* Reconcile XLog Eof between primary and mirror */

	FileRepOperationRename,
		/* Rename flat file on a mirror relation */

	FileRepOperationShutdown,
		/* Transition a mirror to shutdown */

	FileRepOperationInSyncTransition,
		/* Transition a mirror from InResync to InSync */

	FileRepOperationHeartBeat,
		/* Verify that flow from primary to mirror and back is alive */

	FileRepOperationCreateAndOpen,
		/* Create and Open mirror relation */

	FileRepOperationValidation,
		/* Validate filespace directory */

	FileRepOperationDropFilesFromDir,
		/* Drop Files from specified directory */

	FileRepOperationDropTemporaryFiles,
		/* Drop temporary files */

	FileRepOperationStartSlruChecksum,
		/* Start computing the checksum for a SLRU directory on mirror */

	FileRepOperationVerifySlruDirectoryChecksum,
		/* Verify checksum of a directory for a SLRU directory on mirror */

	/*
	  IMPORTANT: If add new operation, add to FileRepOperationToString

	  Also a number of decisions should be made about this operation:

	  1) Is the operation synchronous?
	  Answer True/False with addition to FileRep_IsOperationSynchronous in cdbfilerep.c

	  2) Should this operation carry a description field from primary to mirror?
	  Answer by adding to switch statement in FileRepPrimary_ConstructAndInsertMessage
	  Could encapsulate this decision as a bool routine like IsOperation Synchronous

	  3) Should this operation carry a description field from mirror back to the primary?
	  Answer by adding to switch statement in FileRepAckMirror_ConstructAndInsertMessage
	  Encapsulate this decision as a bool routine like IsOperation Synchronous?

	  4) Do you want to define a unique message description for this operation?
	  If so add struct below and then add struct into union for all operations (FileRepOperationDescription_u)

	  5) Does this operation carry a message body?
	  If so in FileRepMirror_RunConsumer, you need to calcuate and check the CRC over the message body
      Could encapsulate this descision as a bool routine like IsOperationSynchronous? DoesOperationCarryMessaageBody
	*/

	FileRepOperationNotSpecified,

	/* the number of values in this enumeration */
	FileRepOperation__EnumerationCount

} FileRepOperation_e;

/*
 *
 */
typedef enum FileRepAckState_e {
	FileRepAckStateNotInitialized=0,

	FileRepAckStateWaiting,

	FileRepAckStateCompleted,

	FileRepAckStateMirrorInFault,


	/* the number of values in this enumeration */
	FileRepAckState__EnumerationCount

} FileRepAckState_e;


/*
 *
 */
typedef enum FileRepResyncState_e {
	FileRepResyncStateNotInitialized=0,

	FileRepResyncStateInitialized,
		/* entry is initialized and file waited to be resynced */

	FileRepResyncStateInProgress,
		/* file is in resync */

	FileRepResyncStateCompleted,
		/* file resync completed */

	FileRepResyncStateFault,
		/* file resync fault */

} FileRepResyncState_e;

/*
 * This structure identifies uniquely Append Only,
 * Buffer Pool, and Bulk load relation.
 */
typedef struct FileRepRelationIdentifier_s {
	RelFileNode	relFileNode;
		/* The tablespace, database, and relation OIDs for the open. */

	uint32		segmentFileNum;
		/* Which segment file? */

	char		mirrorFilespaceLocation[MAXPGPATH+1];
		/* Mirror Filespace Location */

} FileRepRelationIdentifier_s;

/*
 * This structure identifies uniquely flat file.
 */
typedef struct FileRepFlatFileIdentifier_s {
	char	directorySimpleName[MAXPGPATH+1];
		/* The simple name of the directory in the instance database directory. */

	char	fileSimpleName[MAXPGPATH+1];
		/* The simple name of the file. */

	int32 blockId;
} FileRepFlatFileIdentifier_s;

/*
 * Directory types to be created/dropped on mirror
 */
typedef enum FileRepDirType_e {
	FileRepDirNotInitialized=0,

	FileRepDirFilespace,
		/* filespace directory */

	FileRepDirTablespace,
		/* tablespace directory */

	FileRepDirDatabase,
		/* database directory */

} FileRepDirType_e;

/*
 * Validation type to be performed on mirror
 */
typedef enum FileRepValidationType_e {
	FileRepValidationNotInitialized=0,

	FileRepValidationFilespace,
		/* Validate if filespace directory can be created. */

	FileRepValidationExistence,
		/*
		 * Validate if directory or file exists on mirror.
		 * Check is done during resynchronization with full copy.
		 */

} FileRepValidationType_e;

/*
 * This structure identifies uniquely directory
 * to be created/dropped on mirror.
 */
typedef struct FileRepDirIdentifier_s {
	FileRepDirType_e	fileRepDirType;
	/* which directory */

	DbDirNode			dbDirNode;
	/* The tablespace and database OIDs */

	char				mirrorFilespaceLocation[MAXPGPATH+1];
	/* Mirror Filespace Location */

} FileRepDirIdentifier_s;

/*
 * This structure is used for control messages
 * (i.e. FileRepOperationShutdown)
 */
typedef struct FileRepUnknownIdentifier_s {

	char		unknownId[FILEREP_UNKNOWN_IDENTIFIER_LEN+1];
} FileRepUnknownIdentifier_s;

/*
 * This structure identifies uniquely receiver of the message.
 */
typedef union FileRepIdentifier_u {
	FileRepRelationIdentifier_s fileRepRelationIdentifier;
		/*  */

	FileRepFlatFileIdentifier_s fileRepFlatFileIdentifier;
		/* */

//    FileRepFlatFileBlockIdentifier_s fileRepFlatFileBlockIdentifier;

	FileRepDirIdentifier_s		fileRepDirIdentifier;
		/* */

	FileRepUnknownIdentifier_s	fileRepUnknownIdentifier;
		/* */

} FileRepIdentifier_u;

/*
 *
 */
typedef struct FileRepOperationDescriptionOpen_s {
	int			fileFlags;
		/* file flags */

	int			fileMode;
		/* file mode */

	int64		logicalEof;

	bool		suppressError;

} FileRepOperationDescriptionOpen_s;

/*
 *
 */
typedef struct FileRepOperationDescriptionTruncate_s {
	int64			position;
	/* the position to cutoff the data */

} FileRepOperationDescriptionTruncate_s;

/*
 *
 */
typedef struct FileRepOperationDescriptionWrite_s {
	int32		offset;
		/* blocknum if buffer pool */

	uint32		dataLength;
		/* 32k for buffer pool */

	XLogRecPtr	lsn;
		/* */
} FileRepOperationDescriptionWrite_s;

/*
 *
 */
typedef struct FileRepOperationDescriptionReconcile_s {

	XLogRecPtr	xLogEof;
		/* XLog logical eof */

} FileRepOperationDescriptionReconcile_s;

typedef struct FileRepOperationDescriptionRename_s {

	FileRepIdentifier_u fileRepIdentifier;
	/* new identifier (after rename). */

} FileRepOperationDescriptionRename_s;

typedef struct FileRepOperationDescriptionHeartBeat_s {

	FileRepConsumerProcIndex_e fileRepConsumerProcIndex;
	/* identify shared memory to verify the flow on mirror  */

} FileRepOperationDescriptionHeartBeat_s;

/*
 *
 */
typedef struct FileRepOperationDescriptionValidation_s
{
	FileRepValidationType_e	fileRepValidationType;

	int		mirrorStatus;

} FileRepOperationDescriptionValidation_s;

/*
 *
 */
typedef struct FileRepOperationDescriptionCreate_s
{
	int		ignoreAlreadyExists;

	int		mirrorStatus;

} FileRepOperationDescriptionCreate_s;


/*
 * Structure for the FileRepOperationStartSlruChecksum command
 */
typedef struct FileRepOperationDescriptionStartChecksum_s
{
	int  mirrorStatus;

} FileRepOperationDescriptionStartChecksum_s;

/*
 * Structure for the FileRepOperationVerifySlruDirectoryChecksum command
 */
typedef struct FileRepOperationDescriptionVerifyDirectoryChecksum_s
{
	char md5[SLRU_MD5_BUFLEN];            /* md5 of the primary checksum file */

	int mirrorStatus;

} FileRepOperationDescriptionVerifyDirectoryChecksum_s;

typedef enum FileRepOperationVerificationType_e
{
	FileRepOpVerifyType_NotInitialized=0,

	FileRepOpVerifyType_SummaryItems,
	FileRepOpVerifyType_EnhancedBlockHash,

	FileRepOpVerifyType_RequestParameters,
	/* Not really used now, but could be useful for repairs
	   if we decided to do those
	*/
	FileRepOpVerifyType_DirectoryContents,

	/*
	   If add an operation type, then should add corresponding structure into the
	   FileRepOperationDescriptionVerify_u and add string into FileRepOperationVerificationTypeToString
	*/

	/* the number of values in this enumeration */
	FileRepOpVerifyType__EnumerationCount

} FileRepOperationVerificationType_e;

/*
 *
 */

typedef enum FileRepOperationVerifyParameterType_e
{
	FileRepOpVerifyParamterType_IgnoreInfo

} FileRepOperationVerifyParameterType_e;

typedef struct FileRepOperationDescriptionVerify_RequestParameters_s
{
	char  token[FILEREP_VERIFY_MAX_REQUEST_TOKEN_LEN+1];
	FileRepOperationVerifyParameterType_e type;
	int numFileIgnore;
	int numDirIgnore;

} FileRepOperationDescriptionVerify_RequestParameters_s;


typedef struct FileRepOperationDescriptionVerify_SummaryItems_s
{
	//when sent from primary to mirror this is the number of things to be verified
	//when sent from mirror to primary this is the number of things that mismatched
	uint32          num_entries;
	bool            hashcodesValid;
	bool            sizesValid;

} FileRepOperationDescriptionVerify_SummaryItems_s;

typedef struct FileRepOperationDescriptionVerify_DirectoryContents_Primary_s
{
	bool            firstBatch;

} FileRepOperationDescriptionVerify_DirectoryContents_Primary_s;

typedef struct FileRepOperationDescriptionVerify_DirectoryContents_Mirror_s
{
	uint32          numChildrenInBatch;
	bool            startOver;
	bool            finalBatch;
	bool            directoryExists;

} FileRepOperationDescriptionVerify_DirectoryContents_Mirror_s;



typedef enum FileRepVerify_CompareException_e
{
	FileRepVerifyCompareException_None=0,
	FileRepVerifyCompareException_AORelation,
	FileRepVerifyCompareException_HeapRelation,
	FileRepVerifyCompareException_BtreeIndex

} FileRepVerify_CompareException_e;


typedef struct FileRepOperationDescriptionVerify_EnhancedBlockHash_s
{
	uint64		offset;
	uint64		dataLength; //amount of data to be compute the hash over

	pg_crc32		hashcode;

	FileRepVerify_CompareException_e exception;

	bool isOnMirror;
	bool lastBlock;

} FileRepOperationDescriptionVerify_EnhancedBlockHash_s;



typedef struct FileRepOperationDescriptionVerify_u
{
	FileRepOperationDescriptionVerify_SummaryItems_s summaryItems;
	FileRepOperationDescriptionVerify_EnhancedBlockHash_s enhancedBlockHash;
	FileRepOperationDescriptionVerify_RequestParameters_s requestParameters;

	//Both used to support 	FileRepOpVerifyType_DirectoryContents - one for what primary sends to mirror and one for what mirror send back
	FileRepOperationDescriptionVerify_DirectoryContents_Primary_s dirContents;
	FileRepOperationDescriptionVerify_DirectoryContents_Mirror_s dirContentsMirror;

} FileRepOperationDescriptionVerify_u;

typedef struct FileRepOperationDescriptionDefault_s {
	bool placeholder;
} FileRepOperationDescriptionDefault_s;

/*
 *
 */
typedef union FileRepOperationDescription_u
{

	FileRepOperationDescriptionOpen_s open;
		/* */

	FileRepOperationDescriptionTruncate_s truncate;
		/* */

	FileRepOperationDescriptionWrite_s write;
		/* */

	FileRepOperationDescriptionReconcile_s reconcile;

	FileRepOperationDescriptionRename_s rename;

	FileRepOperationDescriptionHeartBeat_s heartBeat;

	FileRepOperationDescriptionValidation_s validation;

	FileRepOperationDescriptionCreate_s create;

	FileRepOperationDescriptionStartChecksum_s startChecksum;

	FileRepOperationDescriptionVerifyDirectoryChecksum_s verifyDirectoryChecksum;

	FileRepOperationDescriptionDefault_s defaultDesc;
} FileRepOperationDescription_u;

/*
 * This structure describes operation to be mirrored.
 */
typedef struct FileRepMessageHeader_s {
	/* NOTE probably this field can be removed, we are checking protocol
	  version  during handshake */
	FileRepMessageHeaderVersion_e fileRepMessageHeaderVersion;
		/* Message Header Version */

	FileRepRelationType_e fileRepRelationType;
		/* Mirror Relation type. */

	FileRepOperation_e fileRepOperation;
		/* Operation to be issued on a mirror. */

	FileRepIdentifier_u fileRepIdentifier;
		/* Message Receiver identifier. */

	FileRepOperationDescription_u fileRepOperationDescription;
		/* Describes metadata required to perform operation */

	pg_crc32 fileRepMessageBodyCrc;
		/* CRC of the message body */

	FileRepAckState_e fileRepAckState;
		/* state/status of the ACK message */

	uint32	messageCount;

	uint32 spareField;

	uint32 messageBodyLength;
		/* reserved for future */

} FileRepMessageHeader_s;

typedef struct FileRepMessage_s {
	FileRepMessageHeader_s fileRepMessageHeader;
		/* */

	pg_crc32	fileRepMessageHeaderCrc;
		/* */

	char		fileRepMessageBody[FILEREP_MESSAGEBODY_LEN+1];
		/* */

} FileRepMessage_s;

/*********************************************************
 * FILE REP PROCESSES
 *********************************************************/

typedef enum FileRepProcessType_e {
	FileRepProcessTypeNotInitialized=0,

	FileRepProcessTypeMain,
	/* Main File Rep process forked by Postmaster. */

	FileRepProcessTypePrimarySender,
	/* Primary Sender process forked by main file rep process. */

	FileRepProcessTypeMirrorReceiver,
	/* Mirror Receiver process forked by main file rep process. */

	FileRepProcessTypeMirrorConsumer,
	/* Mirror Consumer process forked by main file rep process. */

	FileRepProcessTypeMirrorSenderAck,
	/* Mirror Sender ACK process forked by main file rep process. */

	FileRepProcessTypePrimaryReceiverAck,
	/* Primary Receiver ACK process forked by main file rep process. */

	FileRepProcessTypePrimaryConsumerAck,
	/* Primary Consumer ACK process forked by main file rep process. */

	FileRepProcessTypePrimaryRecovery,
	/* Recovery process that runs only during recovery time at gpstart */

	FileRepProcessTypeMirrorConsumerWriter,

	FileRepProcessTypeMirrorConsumerAppendOnly1,

	FileRepProcessTypeResyncManager,

	FileRepProcessTypeResyncWorker1,

	FileRepProcessTypeResyncWorker2,

	FileRepProcessTypeResyncWorker3,

	FileRepProcessTypeResyncWorker4,

/*
	  IMPORTANT: If add new process type, add to FileRepProcessTypeToString

	  Also a number of decisions should be made about this process:

	  1) Is the process a backend sub process?
	  Answer True/False with addition to FileRepIsBackendSubProcess

	  2) Should the process be on the mirror or primary?
	  Answer by starting in the appropriate case in switch for fileRepRole in routine FileRep_Main

	  3) Consider increasing MaxFileRepSubProc

	  4) Add into the FileRepSubProcList

	  5) Add into the proper place in FileRepSubProcess_Main

	  6) For things on mirror if FileRepSubProcess_Main calls FileRepMirror_StartConsumer, then add
	  proper handling into FileRepMirror_StartConsumer



	*/

	FileRepProcessType__EnumerationCount
	/* the count of enumeration values */

} FileRepProcessType_e;

/*
 * Postmaster forks FileRep process. Input parameters
 * are stored in shared memory. Postmaster-backend process
 * writes input parameters. FileRep and Postmaster processes
 * read them (never write).
 *
 * INPUT parameters
 *			Replica Role
 *			Replica State
 *			Primary IP address
 *			Mirror IP address
 *			Primary Port
 *			Mirror Port
 */
extern void FileRep_Main(void);

/*
 * Set state in FileRep process and sent signal to postmaster
 * (if required).
 */
extern void FileRep_SetState(
				FileRepState_e fileRepStateLocal);

/*
 * Return state of FileRep process.
 */
extern FileRepState_e FileRep_GetState(void);

extern bool FileRep_IsInRecovery(void);

extern bool FileRep_IsInResynchronizeReady(void);

extern bool FileRep_IsInChangeTracking(void);

/*
 * change data state of the segment and inform postmaster to
 * signal all segment processes
 */
extern void FileRep_SetDataState(
								 DataState_e dataState,
								 bool signalPostmaster);

/*
 * change segment state and inform postmaster in order to signal
 * all segment processes
 */
extern void FileRep_SetSegmentState(SegmentState_e segmentState, FaultType_e faultType);

extern void FileRep_SetPostmasterReset(void);
/*
 * Reserve memory in order to insert FileRep message.
 * The routine returns the insert position in FileRep shared memory.
 */
extern char *
FileRep_ReserveShmem(
					 FileRepShmem_s		*fileRepShmem,
					 uint32				msgLength,
					 uint32				*spareField,
					 FileRepOperation_e	fileRepOperation,
					 LWLockId whichLock);

/*
 * ERROR REPORTING
 */

extern int FileRep_errcontext(void);

extern int FileRep_errdetail(
				  FileRepIdentifier_u		fileRepIdentifier,
				  FileRepRelationType_e		fileRepRelationType,
				  FileRepOperation_e		fileRepOperation,
				  int						messageCount);

extern int FileRep_ReportRelationPath(
							   char*		filespaceLocation,
							   RelFileNode	relFileNode,
							   uint32		segmentFileNum);

extern int FileRep_errdetail_Shmem(void);

extern int FileRep_errdetail_ShmemAck(void);

/*********************************************************
 * FILEREP IN SHARED MEMORY LOG
 *********************************************************/
/*
 * Wraparound configuration log for troubleshooting is maintained by each segment.
 */
extern Size FileRepLog_ShmemSize(void);

extern void FileRepLog_ShmemInit(void);

extern	void FileRep_InsertLogEntry(
									char					*routineName,
									FileRepIdentifier_u		fileRepIdentifier,
									FileRepRelationType_e	fileRepRelationType,
									FileRepOperation_e		fileRepOperation,
									pg_crc32				fileRepMessageHeaderCrc,
									pg_crc32				fileRepMessageBodyCrc,
									FileRepAckState_e		fileRepAckState,
									uint32					localCount,
									uint32					fileRepMessageCount);

#define FileRep_InsertConfigLogEntry(description)	\
			FileRep_InsertConfigLogEntryInternal(description, __FILE__, __LINE__, PG_FUNCNAME_MACRO)

extern void	FileRep_InsertConfigLogEntryInternal(char		*description,
												 const char *fileName,
												 int		lineNumber,
												 const char *funcName);

extern void FileRep_Sleep1ms(int retry);
extern void FileRep_Sleep10ms(int retry);

/*
 * Calculate 32 bit Crc
 */
extern void FileRep_CalculateCrc(
						char		*data,
						uint32		length,
						pg_crc32	*crc);

extern FileRepIdentifier_u FileRep_GetRelationIdentifier(
							  char			*mirrorFilespaceLocation,
							  RelFileNode	relFileNode,
							  int32			segmentFileNum);

extern FileRepIdentifier_u FileRep_GetDirFilespaceIdentifier(
							char	*mirrorFilespaceLocation);

extern FileRepIdentifier_u FileRep_GetDirTablespaceIdentifier(
						    char	*mirrorFilespaceLocation,
						    Oid		tablespace);

extern FileRepIdentifier_u FileRep_GetDirDatabaseIdentifier(
							char		*mirrorFilespaceLocation,
							DbDirNode	dbDirNode);

extern FileRepIdentifier_u FileRep_GetFlatFileIdentifier(
							char*	subDirectory,
							char*	simpleFileName);

extern FileRepIdentifier_u FileRep_GetUnknownIdentifier(
							 char*	unknownId);

extern bool FileRep_IsOperationSynchronous(
						FileRepOperation_e fileRepOperation);

extern bool FileRep_IsIpcSleep(FileRepOperation_e fileRepOperation);

extern XLogRecPtr FileRep_GetXLogRecPtrUndefined(void);


extern FileName
FileRep_GetHashKey(
					  FileRepIdentifier_u	 fileRepIdentifier,
					  FileRepRelationType_e	fileRepRelationType);

extern FileName FileRep_GetFileName(
				 FileRepIdentifier_u	fileRepIdentifier,
				 FileRepRelationType_e	fileRepRelationType);

extern void FileRep_GetRelationPath(
						char		*relationPath,
						RelFileNode relFileNode,
						uint32		segmentFileNum);

/**
 * return true if the process of the given type acts as a backend (and so must be treated differently --
 *   shutdown before the bgwriter shuts down, for example)
 */
extern bool FileRepIsBackendSubProcess(FileRepProcessType_e processType);

extern void FileRep_SetFileRepRetry(void);

extern void FileRep_resetSpinLocks(void);

#endif   /* CDBFILEREP_H */

