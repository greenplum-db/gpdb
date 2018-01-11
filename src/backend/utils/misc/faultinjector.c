/*-------------------------------------------------------------------------
 *
 * faultinjector.c
 *	  GP Fault Injector utility (gpfaultinjector python script) is used 
 *	  for Greenplum internal testing only. 
 * 
 * The utility inject faults (as defined by 'fault_type') on primary or
 * mirror segment at predefined 'fault_name. 
 * 
 * The utility is started on master host.  Master host sends the fault
 * injection request to specified segment.  It connects to postmaster on a
 * segment.  Postmaster spawns backend process that sets fault injection
 * request into shared memory.  Shared memory is accessible to all segment
 * processes.  Segment processes are checking shared memory to find if/when
 * fault has to be injected.
 *
 * Portions Copyright (c) 2009-2010 Greenplum Inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/misc/faultinjector.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <signal.h>

#include "access/xact.h"
#include "cdb/cdbfilerep.h"
#include "cdb/cdbfilerepservice.h"
#include "cdb/cdbresynchronizechangetracking.h"
#include "cdb/cdbutil.h"
#include "postmaster/bgwriter.h"
#include "postmaster/fts.h"
#include "storage/spin.h"
#include "storage/shmem.h"
#include "utils/faultinjector.h"
#include "utils/hsearch.h"
#include "miscadmin.h"

#ifdef FAULT_INJECTOR

/*
 * gettext() can't be used in a static initializer... This breaks nls builds.
 * So, to work around this issue, I've made _() be a no-op.
 */
#undef _
#define _(x) x

typedef struct FaultInjectorShmem_s {
	slock_t		lock;
	
	int			faultInjectorSlots;	
		/* number of fault injection set */
	
	HTAB		*hash;
} FaultInjectorShmem_s;

static	FaultInjectorShmem_s *faultInjectorShmem = NULL;

static void FiLockAcquire(void);
static void FiLockRelease(void);

static FaultInjectorEntry_s* FaultInjector_LookupHashEntry(
								const char* faultName);

static FaultInjectorEntry_s* FaultInjector_InsertHashEntry(
								const char* faultName,
								bool	*exists);

static int FaultInjector_NewHashEntry(
								FaultInjectorEntry_s	*entry);

static int FaultInjector_UpdateHashEntry(
								FaultInjectorEntry_s	*entry);

static bool FaultInjector_RemoveHashEntry(
								const char* faultName);

/* Arrays to map between enum values and strings */
const char*
FaultInjectorTypeEnumToString[] = {
#define FI_TYPE(id, str) str,
#include "utils/faultinjector_lists.h"
#undef FI_TYPE
};

const char*
FaultInjectorIdentifierEnumToString[] = {
#define FI_IDENT(id, str) str,
#include "utils/faultinjector_lists.h"
#undef FI_IDENT
};

const char*
FaultInjectorDDLEnumToString[] = {
#define FI_DDL_STATEMENT(id, str) str,
#include "utils/faultinjector_lists.h"
#undef FI_DDL_STATEMENT
};

const char*
FaultInjectorStateEnumToString[] = {
#define FI_STATE(id, str) str,
#include "utils/faultinjector_lists.h"
#undef FI_STATE
};

/*
 *
 */
FaultInjectorType_e
FaultInjectorTypeStringToEnum(
							  char*		faultTypeString)
{
	FaultInjectorType_e	faultTypeEnum = FaultInjectorTypeMax;
	int	ii;
	
	for (ii=0; ii < FaultInjectorTypeMax; ii++) {
		if (strcmp(FaultInjectorTypeEnumToString[ii], faultTypeString) == 0) {
			faultTypeEnum = ii;
			break;
		}
	}
	return faultTypeEnum;
}

/*
 *
 */
FaultInjectorIdentifier_e
FaultInjectorIdentifierStringToEnum(
									char*	faultName)
{
	FaultInjectorIdentifier_e	faultId = FaultInjectorIdMax;
	int	ii;
	
	for (ii=0; ii < FaultInjectorIdMax; ii++) {
		if (strcmp(FaultInjectorIdentifierEnumToString[ii], faultName) == 0) {
			faultId = ii;
			break;
		}
	}
	return faultId;
}

/*
 *
 */
DDLStatement_e
FaultInjectorDDLStringToEnum(
									char*	ddlString)
{
	DDLStatement_e	ddlEnum = DDLMax;
	int	ii;
	
	for (ii=0; ii < DDLMax; ii++) {
		if (strcmp(FaultInjectorDDLEnumToString[ii], ddlString) == 0) {
			ddlEnum = ii;
			break;
		}
	}
	return ddlEnum;
}

static void
FiLockAcquire(void)
{	
	SpinLockAcquire(&faultInjectorShmem->lock);
}

static void
FiLockRelease(void)
{	
	SpinLockRelease(&faultInjectorShmem->lock);
}

/****************************************************************
 * FAULT INJECTOR routines
 ****************************************************************/
Size
FaultInjector_ShmemSize(void)
{
	Size	size;
	
	size = hash_estimate_size(
							  (Size)FAULTINJECTOR_MAX_SLOTS, 
							  sizeof(FaultInjectorEntry_s));
	
	size = add_size(size, sizeof(FaultInjectorShmem_s));
	
	return size;	
}

/*
 * Hash table contains fault injection that are set on the system waiting to be injected.
 * FaultInjector identifier is the key in the hash table.
 * Hash table in shared memory is initialized only on primary and mirror segment. 
 * It is not initialized on master host.
 */
void
FaultInjector_ShmemInit(void)
{
	HASHCTL	hash_ctl;
	bool	foundPtr;
	
	faultInjectorShmem = (FaultInjectorShmem_s *) ShmemInitStruct("fault injector",
																  sizeof(FaultInjectorShmem_s),
																  &foundPtr);
	
	if (faultInjectorShmem == NULL) {
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 (errmsg("not enough shared memory for fault injector"))));
	}	
	
	if (! foundPtr) 
	{
		MemSet(faultInjectorShmem, 0, sizeof(FaultInjectorShmem_s));
	}	
	
	SpinLockInit(&faultInjectorShmem->lock);
	
	faultInjectorShmem->faultInjectorSlots = 0;
	
	MemSet(&hash_ctl, 0, sizeof(hash_ctl));
	hash_ctl.keysize = FAULT_NAME_MAX_LENGTH;
	hash_ctl.entrysize = sizeof(FaultInjectorEntry_s);
	hash_ctl.hash = string_hash;
	
	faultInjectorShmem->hash = ShmemInitHash("fault injector hash",
								   FAULTINJECTOR_MAX_SLOTS,
								   FAULTINJECTOR_MAX_SLOTS,
								   &hash_ctl,
								   HASH_ELEM | HASH_FUNCTION);
	
	if (faultInjectorShmem->hash == NULL) {
		ereport(ERROR, 
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 (errmsg("not enough shared memory for fault injector"))));
	}
	
	return;						  
}

FaultInjectorType_e
FaultInjector_InjectFaultIfSet(
							   FaultInjectorIdentifier_e identifier,
							   DDLStatement_e			 ddlStatement,
							   const char*					 databaseName,
							   const char*					 tableName)
{
	return FaultInjector_InjectFaultNameIfSet(
			FaultInjectorIdentifierEnumToString[identifier],
			ddlStatement,
			databaseName,
			tableName);
	
}

FaultInjectorType_e
FaultInjector_InjectFaultNameIfSet(
							   const char*				 faultName,
							   DDLStatement_e			 ddlStatement,
							   const char*				 databaseName,
							   const char*				 tableName)
{

	FaultInjectorEntry_s   *entryShared, localEntry,
						   *entryLocal = &localEntry;
	char					databaseNameLocal[NAMEDATALEN];
	char					tableNameLocal[NAMEDATALEN];
	int						ii = 0;
	int cnt = 3600;
	FaultInjectorType_e retvalue = FaultInjectorTypeNotSpecified;

	/*
	 * Return immediately if no fault has been injected ever.  It is
	 * important to not touch the spinlock, especially if this is the
	 * postmaster process.  If one of the backend processes dies while
	 * holding the spin lock, and postmaster comes here before resetting
	 * the shared memory, it waits without holder process and eventually
	 * goes into PANIC.  Also this saves a few cycles to acquire the spin
	 * lock and look into the shared hash table.
	 *
	 * Although this is a race condition without lock, a false negative is
	 * ok given this framework is purely for dev/testing.
	 */
	if (faultInjectorShmem->faultInjectorSlots == 0)
		return FaultInjectorTypeNotSpecified;

	snprintf(databaseNameLocal, sizeof(databaseNameLocal), "%s", databaseName);
	snprintf(tableNameLocal, sizeof(tableNameLocal), "%s", tableName);
	getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);

	FiLockAcquire();

	entryShared = FaultInjector_LookupHashEntry(faultName);

	do
	{
		if (entryShared == NULL)
			/* fault injection is not set */
			break;

		if (entryShared->ddlStatement != ddlStatement)
			/* fault injection is not set for the specified DDL */
			break;

		if (strcmp(entryShared->databaseName, databaseNameLocal) != 0)
			/* fault injection is not set for the specified database name */
			break;
	
		if (strcmp(entryShared->tableName, tableNameLocal) != 0)
			/* fault injection is not set for the specified table name */
			break;

		if (entryShared->faultInjectorState == FaultInjectorStateCompleted ||
			entryShared->faultInjectorState == FaultInjectorStateFailed) {
			/* fault injection was already executed */
			break;
		}

		/* Update the injection fault entry in hash table */
		if (entryShared->occurrence != FILEREP_UNDEFINED)
		{
			if (entryShared->occurrence > 1)
			{
				entryShared->occurrence--;
				break;
			}
		}

		entryShared->faultInjectorState = FaultInjectorStateTriggered;
		entryShared->numTimesTriggered++;
		memcpy(entryLocal, entryShared, sizeof(FaultInjectorEntry_s));
		retvalue = entryLocal->faultInjectorType;
	} while (0);

	FiLockRelease();

	if (retvalue == FaultInjectorTypeNotSpecified)
		return FaultInjectorTypeNotSpecified;

	/* Inject fault */
	
	switch (entryLocal->faultInjectorType) {
		case FaultInjectorTypeNotSpecified:
			
			break;
		case FaultInjectorTypeSleep:
			ereport(LOG,
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));	
			
			pg_usleep(entryLocal->sleepTime * 1000000L);
			break;
		case FaultInjectorTypeFault:
			
			switch (entryLocal->faultInjectorIdentifier)
			{	
				case FileRepConsumer:
				case FileRepConsumerVerification:
				case FileRepSender:
				case FileRepReceiver:
				case FileRepResync:
				case FileRepResyncInProgress:
				case FileRepResyncWorker:
				case FileRepResyncWorkerRead:
				case FileRepTransitionToInResyncMirrorReCreate:
				case FileRepTransitionToInResyncMarkReCreated:
				case FileRepTransitionToInResyncMarkCompleted:
				case FileRepTransitionToInSyncBegin:
				case FileRepTransitionToInSync:
				case FileRepTransitionToInSyncMarkCompleted:
				case FileRepTransitionToInSyncBeforeCheckpoint:
				case FileRepIsOperationCompleted:
			
					FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
					break;
					
				case FileRepTransitionToChangeTracking:

					FileRep_SetPostmasterReset();
					break;

				default:
					
					FileRep_SetSegmentState(SegmentStateFault, FaultTypeIO);
					break;
			}
			ereport(LOG, 
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));	
			
			break;
		case FaultInjectorTypeFatal:
			/*
			 * If it's one time occurrence then disable the fault before it's
			 * actually triggered because this fault errors out the transaction
			 * and hence we wont get a chance to disable it or put it in completed
			 * state.
			 */
			if (entryLocal->occurrence != FILEREP_UNDEFINED)
			{
				entryLocal->faultInjectorState = FaultInjectorStateCompleted;
				FaultInjector_UpdateHashEntry(entryLocal);
			}
			
			ereport(FATAL, 
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));	

			break;
		case FaultInjectorTypePanic:
			/*
			 * If it's one time occurrence then disable the fault before it's
			 * actually triggered because this fault errors out the transaction
			 * and hence we wont get a chance to disable it or put it in completed
			 * state. For PANIC it may be unnecessary though.
			 */
			if (entryLocal->occurrence != FILEREP_UNDEFINED)
			{
				entryLocal->faultInjectorState = FaultInjectorStateCompleted;
				FaultInjector_UpdateHashEntry(entryLocal);
			}
			
			ereport(PANIC, 
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));	

			break;
		case FaultInjectorTypeError:
			/*
			 * If it's one time occurrence then disable the fault before it's
			 * actually triggered because this fault errors out the transaction
			 * and hence we wont get a chance to disable it or put it in completed
			 * state.
			 */
			if (entryLocal->occurrence != FILEREP_UNDEFINED)
			{
				entryLocal->faultInjectorState = FaultInjectorStateCompleted;
				FaultInjector_UpdateHashEntry(entryLocal);
			}

			ereport(ERROR, 
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));	
			break;
		case FaultInjectorTypeInfiniteLoop:
			ereport(LOG, 
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));
			if (entryLocal->faultInjectorIdentifier == FileRepImmediateShutdownRequested)
				cnt = entryLocal->sleepTime;

			for (ii=0; ii < cnt; ii++)
			{
				pg_usleep(1000000L); // sleep for 1 sec (1 sec * 3600 = 1 hour)
				
				getFileRepRoleAndState(NULL, &segmentState, NULL, NULL, NULL);

				if ((entryLocal->faultInjectorIdentifier != FileRepImmediateShutdownRequested) &&
					(segmentState == SegmentStateShutdownFilerepBackends ||
					segmentState == SegmentStateImmediateShutdown ||
					segmentState == SegmentStateShutdown ||
					IsFtsShudownRequested()))
				{
					break;
				}
			}
			break;
		case FaultInjectorTypeDataCorruption:
			ereport(LOG, 
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));							
			break;
			
		case FaultInjectorTypeSuspend:
		{
			FaultInjectorEntry_s	*entry;
			
			ereport(LOG, 
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));	
			
			while ((entry = FaultInjector_LookupHashEntry(entryLocal->faultName)) != NULL &&
				   entry->faultInjectorType != FaultInjectorTypeResume)
			{
				pg_usleep(1000000L);  // 1 sec
			}

			if (entry != NULL)
			{
				ereport(LOG, 
						(errcode(ERRCODE_FAULT_INJECT),
						 errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entry->faultInjectorType])));	
			}
			else
			{
				ereport(LOG, 
						(errcode(ERRCODE_FAULT_INJECT),
						 errmsg("fault 'NULL', fault name:'%s'  ",
								entryLocal->faultName)));

				/*
				 * Since the entry is gone already, we should NOT update
				 * the entry below.  (There could be other places in this
				 * function that are under the same situation, but I'm too
				 * tired to look for them...)
				 */
				return entryLocal->faultInjectorType;
			}
			break;
		}
		case FaultInjectorTypeSkip:
			ereport(LOG, 
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));							
			break;
			
		case FaultInjectorTypeMemoryFull:
		{
			char	*buffer = NULL;
			
			ereport(LOG, 
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));	

			buffer = (char*) palloc(BLCKSZ);

			while (buffer != NULL)
			{
				buffer = (char*) palloc(BLCKSZ);
			}
			
			break;
		}	
		case FaultInjectorTypeReset:
		case FaultInjectorTypeStatus:
			
			ereport(LOG, 
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("unexpected error, fault triggered, fault name:'%s' fault type:'%s' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));	
			
			Assert(0);
			break;
		case FaultInjectorTypeResume:
			break;
			
		case FaultInjectorTypeSegv:
		{
			*(volatile int *) 0 = 1234;
			break;
		}
		
		case FaultInjectorTypeInterrupt:
		{
			/*
			 * The place where this type of fault is injected must have
			 * has HOLD_INTERRUPTS() .. RESUME_INTERRUPTS() around it, otherwise
			 * the interrupt could be handled inside the fault injector itself
			 */
			ereport(LOG,
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));

			InterruptPending = true;
			QueryCancelPending = true;
			break;
		}

		case FaultInjectorTypeFinishPending:
		{
			ereport(LOG,
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));
			QueryFinishPending = true;
			break;
		}

		case FaultInjectorTypeCheckpointAndPanic:
		{
			if (entryLocal->occurrence != FILEREP_UNDEFINED)
			{
				entryLocal->faultInjectorState = FaultInjectorStateCompleted;
				FaultInjector_UpdateHashEntry(entryLocal);
			}

			RequestCheckpoint(CHECKPOINT_WAIT | CHECKPOINT_IMMEDIATE);
			ereport(PANIC,
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));
			break;
		}

		default:
			
			ereport(LOG, 
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("unexpected error, fault triggered, fault name:'%s' fault type:'%s' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));	
			
			Assert(0);
			break;
	}
		
	if (entryLocal->occurrence != FILEREP_UNDEFINED)
	{
		entryLocal->faultInjectorState = FaultInjectorStateCompleted;
	}

	FaultInjector_UpdateHashEntry(entryLocal);	
	
	return (entryLocal->faultInjectorType);
}

/*
 * lookup if fault injection is set
 */
static FaultInjectorEntry_s*
FaultInjector_LookupHashEntry(
							  const char* faultName)
{
	FaultInjectorEntry_s	*entry;
	
	Assert(faultInjectorShmem->hash != NULL);
	entry = (FaultInjectorEntry_s *) hash_search(
												  faultInjectorShmem->hash, 
												  (void *) faultName, // key
												  HASH_FIND, 
												  NULL);
	
	if (entry == NULL) {
		ereport(DEBUG5,
				(errmsg("FaultInjector_LookupHashEntry() could not find fault injection hash entry:'%s' ",
						faultName)));
	} 
	
	return entry;
}

/*
 * insert fault injection in hash table 
 */ 
static FaultInjectorEntry_s*
FaultInjector_InsertHashEntry(
							const char* faultName,
							bool	*exists)
{
	
	bool					foundPtr;
	FaultInjectorEntry_s	*entry;

	Assert(faultInjectorShmem->hash != NULL);
	entry = (FaultInjectorEntry_s *) hash_search(
												  faultInjectorShmem->hash, 
												  (void *) faultName, // key
												  HASH_ENTER_NULL,
												  &foundPtr);

	if (entry == NULL) {
		*exists = FALSE;
		return entry;
	} 
	
	elog(DEBUG1, "FaultInjector_InsertHashEntry() entry_key:%d", 
		 entry->faultInjectorIdentifier);
	
	if (foundPtr) {
		*exists = TRUE;
	} else {
		*exists = FALSE;
	}

	return entry;
}

/*
 * 
 */
static bool
FaultInjector_RemoveHashEntry(
							  const char* faultName)
{	
	
	FaultInjectorEntry_s	*entry;
	bool					isRemoved = FALSE;
	
	Assert(faultInjectorShmem->hash != NULL);
	entry = (FaultInjectorEntry_s *) hash_search(
												  faultInjectorShmem->hash, 
												  (void *) faultName, // key
												  HASH_REMOVE, 
												  NULL);
	
	if (entry) 
	{
		ereport(LOG, 
				(errmsg("fault removed, fault name:'%s' fault type:'%s' ",
						entry->faultName,
						FaultInjectorTypeEnumToString[entry->faultInjectorType])));							
		
		isRemoved = TRUE;
	}
	
	return isRemoved;			
}

/*
 *
 */
static int 
FaultInjector_NewHashEntry(
						   FaultInjectorEntry_s	*entry)
{
	
	FaultInjectorEntry_s	*entryLocal=NULL;
	bool					exists;
	int						status = STATUS_OK;

	FiLockAcquire();

	if ((faultInjectorShmem->faultInjectorSlots + 1) >= FAULTINJECTOR_MAX_SLOTS) {
		FiLockRelease();
		status = STATUS_ERROR;
		ereport(WARNING,
				(errmsg("could not insert fault injection, no slots available"
						"fault name:'%s' fault type:'%s' ",
						entry->faultName,
						FaultInjectorTypeEnumToString[entry->faultInjectorType])));
		snprintf(entry->bufOutput, sizeof(entry->bufOutput), 
				 "could not insert fault injection, max slots:'%d' reached",
				 FAULTINJECTOR_MAX_SLOTS);
		
		goto exit;
	}
	
	if (entry->faultInjectorType == FaultInjectorTypeSkip)
	{
		switch (entry->faultInjectorIdentifier)
		{
			case Checkpoint:
			case FsyncCounter:
			case BgBufferSyncDefaultLogic:
			case ChangeTrackingDisable:
			case FileRepVerification:

			case FinishPreparedTransactionCommitPass1FromCreatePendingToCreated:
			case FinishPreparedTransactionCommitPass2FromCreatePendingToCreated:
				
			case FinishPreparedTransactionCommitPass1FromDropInMemoryToDropPending:
			case FinishPreparedTransactionCommitPass2FromDropInMemoryToDropPending:
				
			case FinishPreparedTransactionCommitPass1AbortingCreateNeeded:
			case FinishPreparedTransactionCommitPass2AbortingCreateNeeded:

			case FinishPreparedTransactionAbortPass1FromCreatePendingToAbortingCreate:
			case FinishPreparedTransactionAbortPass2FromCreatePendingToAbortingCreate:
				
			case FinishPreparedTransactionAbortPass1AbortingCreateNeeded:
			case FinishPreparedTransactionAbortPass2AbortingCreateNeeded:

			case InterconnectStopAckIsLost:
			case SendQEDetailsInitBackend:
			case ExecutorRunHighProcessed:

				break;
			default:
				
				FiLockRelease();
				status = STATUS_ERROR;
				ereport(WARNING,
						(errmsg("could not insert fault injection, fault type not supported"
								"fault name:'%s' fault type:'%s' ",
								entry->faultName,
								FaultInjectorTypeEnumToString[entry->faultInjectorType])));
				snprintf(entry->bufOutput, sizeof(entry->bufOutput), 
						 "could not insert fault injection, fault type not supported");
				
				goto exit;
		}
	}
	
	/* check role */
	
	getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);
	
	switch (entry->faultInjectorIdentifier)
	{
		case ChangeTrackingDisable:
		case FileRepConsumerVerification:
		case FileRepResync:
		case FileRepResyncInProgress:
		case FileRepResyncWorker:
		case FileRepResyncWorkerRead:
		case FileRepTransitionToInResyncMirrorReCreate:
		case FileRepTransitionToInResyncMarkReCreated:
		case FileRepTransitionToInResyncMarkCompleted:
		case FileRepTransitionToInSyncBegin:
		case FileRepTransitionToInSync:
		case FileRepTransitionToInSyncMarkCompleted:
		case FileRepTransitionToInSyncBeforeCheckpoint:
		case FileRepTransitionToChangeTracking:
		case FileRepIsOperationCompleted:
		case FileRepImmediateShutdownRequested:
		case FileRepChangeTrackingCompacting:
			if (fileRepRole != FileRepPrimaryRole)
			{
				FiLockRelease();
				status = STATUS_ERROR;
				ereport(WARNING,
						(errmsg("could not insert fault injection entry into table, segment not in primary role"
								"fault name:'%s' fault type:'%s' ",
								entry->faultName,
								FaultInjectorTypeEnumToString[entry->faultInjectorType])));
				snprintf(entry->bufOutput, sizeof(entry->bufOutput), 
						 "could not insert fault injection, segment not in primary role");
				
				goto exit;
			}			
			break;
		
		case FileRepConsumer:
		case FileRepSender:
		case FileRepReceiver:
		case FileRepFlush:
			if (fileRepRole != FileRepPrimaryRole && fileRepRole != FileRepMirrorRole)
			{
				FiLockRelease();
				status = STATUS_ERROR;
				ereport(WARNING,
						(errmsg("could not insert fault injection entry into table, "
								"segment not in primary or mirror role, "
								"fault name:'%s' fault type:'%s' ",
								entry->faultName,
								FaultInjectorTypeEnumToString[entry->faultInjectorType])));
				snprintf(entry->bufOutput, sizeof(entry->bufOutput), 
						 "could not insert fault injection, segment not in primary or mirror role");
				
				goto exit;
			}			
			break;
			
		case TransactionAbortAfterDistributedPrepared:
		case DtmBroadcastPrepare:
		case DtmBroadcastCommitPrepared:
		case DtmBroadcastAbortPrepared:
		case DtmXLogDistributedCommit:
		case OptRelcacheTranslatorCatalogAccess:
			
			if (fileRepRole != FileRepNoRoleConfigured)
			{
				FiLockRelease();
				status = STATUS_ERROR;
				ereport(WARNING,
						(errmsg("could not insert fault injection entry into table, "
								"segment not in master role, "
								"fault name:'%s' fault type:'%s' ",
								entry->faultName,
								FaultInjectorTypeEnumToString[entry->faultInjectorType])));
				snprintf(entry->bufOutput, sizeof(entry->bufOutput), 
						 "could not insert fault injection, segment not in master role");
				
				goto exit;
			}			
			break;
			

		case StartPrepareTx:
		case FinishPreparedTransactionCommitPass1FromCreatePendingToCreated:
		case FinishPreparedTransactionCommitPass2FromCreatePendingToCreated:
			
		case FinishPreparedTransactionCommitPass1FromDropInMemoryToDropPending:
		case FinishPreparedTransactionCommitPass2FromDropInMemoryToDropPending:
			
		case FinishPreparedTransactionCommitPass1AbortingCreateNeeded:
		case FinishPreparedTransactionCommitPass2AbortingCreateNeeded:
			
		case FinishPreparedTransactionAbortPass1FromCreatePendingToAbortingCreate:
			
		case FinishPreparedTransactionAbortPass1AbortingCreateNeeded:
		case FinishPreparedTransactionAbortPass2AbortingCreateNeeded:
		case TwoPhaseTransactionCommitPrepared:
		case TwoPhaseTransactionAbortPrepared:
		
		/* We do not use vmem on master. Therefore, we only attempt large palloc on segments. */
		case MultiExecHashLargeVmem:
		case FaultInBackgroundWriterMain:
		case SendQEDetailsInitBackend:
		
		case FaultBeforePendingDeleteRelationEntry:
		case FaultBeforePendingDeleteDatabaseEntry:
		case FaultBeforePendingDeleteTablespaceEntry:
		case FaultBeforePendingDeleteFilespaceEntry:	
		case PgControl:
		case PgXlog:
		case SegmentTransitionRequest:
		case SegmentProbeResponse:

		case LocalTmRecordTransactionCommit:
		case Checkpoint:
		case AbortTransactionFail:
		case WorkfileCreationFail:
		case WorkfileWriteFail:
		case WorkfileHashJoinFailure:
		case UpdateCommittedEofInPersistentTable:
		case ExecSortBeforeSorting:
		case FaultDuringExecDynamicTableScan:
		case FaultExecHashJoinNewBatch:
		case RunawayCleanup:
		case ExecSortMKSortMergeRuns:
		case ExecShareInputNext:
			if (fileRepRole != FileRepNoRoleConfigured && fileRepRole != FileRepPrimaryRole)
			{
				FiLockRelease();
				status = STATUS_ERROR;
				ereport(WARNING,
						(errmsg("could not insert fault injection entry into table, "
								"segment not in primary or master role, "
								"fault name:'%s' fault type:'%s' ",
								entry->faultName,
								FaultInjectorTypeEnumToString[entry->faultInjectorType])));
				snprintf(entry->bufOutput, sizeof(entry->bufOutput), 
						 "could not insert fault injection, segment not in primary or master role");
				
				goto exit;
			}			
			break;

		default:
			break;
	}
	entryLocal = FaultInjector_InsertHashEntry(entry->faultName, &exists);
		
	if (entryLocal == NULL) {
		FiLockRelease();
		status = STATUS_ERROR;
		ereport(WARNING,
				(errmsg("could not insert fault injection entry into table, no memory, "
						"fault name:'%s' fault type:'%s' ",
						entry->faultName,
						FaultInjectorTypeEnumToString[entry->faultInjectorType])));
		snprintf(entry->bufOutput, sizeof(entry->bufOutput), 
				 "could not insert fault injection, no memory");
		
		goto exit;
	}
		
	if (exists) {
		FiLockRelease();
		status = STATUS_ERROR;
		ereport(WARNING,
				(errmsg("could not insert fault injection entry into table, "
						"entry already exists, "
						"fault name:'%s' fault type:'%s' ",
						entry->faultName,
						FaultInjectorTypeEnumToString[entry->faultInjectorType])));
		snprintf(entry->bufOutput, sizeof(entry->bufOutput), 
				 "could not insert fault injection, entry already exists");
		
		goto exit;
	}
		
	entryLocal->faultInjectorType = entry->faultInjectorType;
	entryLocal->faultInjectorIdentifier = entry->faultInjectorIdentifier;
	strlcpy(entryLocal->faultName, entry->faultName, sizeof(entryLocal->faultName));

	entryLocal->sleepTime = entry->sleepTime;
	entryLocal->ddlStatement = entry->ddlStatement;
	
	if (entry->occurrence != 0)
	{
		entryLocal->occurrence = entry->occurrence;
	}
	else 
	{
		entryLocal->occurrence = FILEREP_UNDEFINED;
	}

	entryLocal->numTimesTriggered = 0;
	strcpy(entryLocal->databaseName, entry->databaseName);
	strcpy(entryLocal->tableName, entry->tableName);
		
	entryLocal->faultInjectorState = FaultInjectorStateWaiting;

	faultInjectorShmem->faultInjectorSlots++;
		
	FiLockRelease();
	
	elog(DEBUG1, "FaultInjector_NewHashEntry() identifier:'%s'", 
		 entry->faultName);
	
exit:
		
	return status;			
}

/*
 * update hash entry with state 
 */		
static int 
FaultInjector_UpdateHashEntry(
							FaultInjectorEntry_s	*entry)
{
	
	FaultInjectorEntry_s	*entryLocal;
	int						status = STATUS_OK;

	FiLockAcquire();

	entryLocal = FaultInjector_LookupHashEntry(entry->faultName);
	
	if (entryLocal == NULL)
	{
		FiLockRelease();
		status = STATUS_ERROR;
		ereport(WARNING,
				(errmsg("could not update fault injection hash entry with fault injection status, "
						"no entry found, "
						"fault name:'%s' fault type:'%s' ",
						entry->faultName,
						FaultInjectorTypeEnumToString[entry->faultInjectorType])));
		goto exit;
	}
	
	if (entry->faultInjectorType == FaultInjectorTypeResume)
	{
		entryLocal->faultInjectorType = FaultInjectorTypeResume;
	}
	else
	{	
		entryLocal->faultInjectorState = entry->faultInjectorState;
		entryLocal->occurrence = entry->occurrence;
	}
	
	FiLockRelease();
	
	ereport(DEBUG1,
			(errmsg("LOG(fault injector): update fault injection hash entry "
					"identifier:'%s' state:'%s' occurrence:'%d' ",
					entry->faultName,
					FaultInjectorStateEnumToString[entryLocal->faultInjectorState],
					entry->occurrence)));
	
exit:	
	
	return status;			
}

/*
 * 
 */
int
FaultInjector_SetFaultInjection(
						   FaultInjectorEntry_s	*entry)
{
	int		status = STATUS_OK;
	bool	isRemoved = FALSE;
	
	getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);

	switch (entry->faultInjectorType) {
		case FaultInjectorTypeReset:
		{
			HASH_SEQ_STATUS			hash_status;
			FaultInjectorEntry_s	*entryLocal;
			
			if (entry->faultInjectorIdentifier == FaultInjectorIdAll) 
			{
				hash_seq_init(&hash_status, faultInjectorShmem->hash);
				
				FiLockAcquire();
				
				while ((entryLocal = (FaultInjectorEntry_s *) hash_seq_search(&hash_status)) != NULL) {
					isRemoved = FaultInjector_RemoveHashEntry(entryLocal->faultName);
					if (isRemoved == TRUE) {
						faultInjectorShmem->faultInjectorSlots--;
					}					
				}
				FiLockRelease();
				Assert(faultInjectorShmem->faultInjectorSlots == 0);
			}
			else
			{
				FiLockAcquire();
				isRemoved = FaultInjector_RemoveHashEntry(entry->faultName);
				if (isRemoved == TRUE) {
					faultInjectorShmem->faultInjectorSlots--;
				}
				FiLockRelease();
			}
				
			if (isRemoved == FALSE) {
				ereport(DEBUG1,
						(errmsg("LOG(fault injector): could not remove fault injection from hash"
								"identifier:'%s' ",
								entry->faultName)));
			}			
			
			break;
		}
		case FaultInjectorTypeStatus:
		{	
			HASH_SEQ_STATUS			hash_status;
			FaultInjectorEntry_s	*entryLocal;
			bool					found = FALSE;
			int                     length;
			
			if (faultInjectorShmem->hash == NULL) {
				status = STATUS_ERROR;
				break;
			}
			length = snprintf(entry->bufOutput, sizeof(entry->bufOutput), "Success: ");
			
			if (entry->faultInjectorIdentifier == ChangeTrackingCompactingReport)
			{
				snprintf(entry->bufOutput, sizeof(entry->bufOutput), 
						 "Success: compacting in progress %s",
						 ChangeTrackingIsCompactingInProgress() ? "true" : "false");
				break;
			}
			
			hash_seq_init(&hash_status, faultInjectorShmem->hash);

			while ((entryLocal = (FaultInjectorEntry_s *) hash_seq_search(&hash_status)) != NULL) {
				ereport(LOG,
					(errmsg("fault injector status: "
							"fault name:'%s' "
							"fault type:'%s' "
							"ddl statement:'%s' "
							"database name:'%s' "
							"table name:'%s' "
							"occurrence:'%d' "
							"sleep time:'%d' "
							"fault injection state:'%s' "
							"num times hit:'%d' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType],
							FaultInjectorDDLEnumToString[entryLocal->ddlStatement],
							entryLocal->databaseName,
							entryLocal->tableName,
							entryLocal->occurrence,
							entryLocal->sleepTime,
							FaultInjectorStateEnumToString[entryLocal->faultInjectorState],
						entryLocal->numTimesTriggered)));
				
				if (entry->faultInjectorIdentifier == entryLocal->faultInjectorIdentifier ||
					entry->faultInjectorIdentifier == FaultInjectorIdAll)
				{
					length = snprintf((entry->bufOutput + length), sizeof(entry->bufOutput) - length,
									  "fault name:'%s' "
									  "fault type:'%s' "
									  "ddl statement:'%s' "
									  "database name:'%s' "
									  "table name:'%s' "
									  "occurrence:'%d' "
									  "sleep time:'%d' "
									  "fault injection state:'%s'  "
									  "num times hit:'%d' \n",
									  entryLocal->faultName,
									  FaultInjectorTypeEnumToString[entryLocal->faultInjectorType],
									  FaultInjectorDDLEnumToString[entryLocal->ddlStatement],
									  entryLocal->databaseName,
									  entryLocal->tableName,
									  entryLocal->occurrence,
									  entryLocal->sleepTime,
									  FaultInjectorStateEnumToString[entryLocal->faultInjectorState],
									  entryLocal->numTimesTriggered);
						found = TRUE;
				}
			}
			if (found == FALSE) {
				snprintf(entry->bufOutput, sizeof(entry->bufOutput), "Failure: "
						 "fault name:'%s' not set",
						 entry->faultName);
			}
			break;
		}
		case FaultInjectorTypeResume:
			ereport(LOG, 
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							entry->faultName,
							FaultInjectorTypeEnumToString[entry->faultInjectorType])));	
			
			FaultInjector_UpdateHashEntry(entry);	
			
			break;
		default: 
			
			status = FaultInjector_NewHashEntry(entry);
			break;
	}
	return status;
}

/*
 * 
 */
bool
FaultInjector_IsFaultInjected(
							  char* faultName)
{
	FaultInjectorEntry_s	*entry = NULL;
	bool					isCompleted = FALSE;
	bool					retval = FALSE;
	bool					isRemoved;
		
	FiLockAcquire();
		
	entry = FaultInjector_LookupHashEntry(faultName);
		
	if (entry == NULL) {
		retval = TRUE;
		isCompleted = TRUE;
		goto exit;
	}
		
	switch (entry->faultInjectorState) {
		case FaultInjectorStateWaiting:
			/* No operation */
			break;
		case FaultInjectorStateTriggered:	
			/* No operation */
			break;
		case FaultInjectorStateCompleted:
			
			retval = TRUE;
			/* NO break */
		case FaultInjectorStateFailed:
			
			isCompleted = TRUE;
			isRemoved = FaultInjector_RemoveHashEntry(faultName);
			
			if (isRemoved == FALSE) {
				ereport(DEBUG1,
						(errmsg("LOG(fault injector): could not remove fault injection from hash"
								"identifier:'%s' ",
								faultName)));
			} else {
				faultInjectorShmem->faultInjectorSlots--;
			}

			break;
		default:
			Assert(0);
	}

exit:
	FiLockRelease();
	
	if ((isCompleted == TRUE) && (retval == FALSE)) {
		ereport(WARNING,
				(errmsg("could not complete fault injection, fault name:'%s' fault type:'%s' ",
						faultName,
						FaultInjectorTypeEnumToString[entry->faultInjectorType])));
	}
	return isCompleted;
}
#endif
