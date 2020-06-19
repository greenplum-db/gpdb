/*-------------------------------------------------------------------------
 *
 * faultinjector.c
 *	  GP Fault Injectors are used for Greenplum internal testing only.
 * 
 * Fault injectors are used for fine control during testing. They allow a
 * developer to create deterministic tests for scenarios that are hard to
 * reproduce. This is done by programming actions at certain key areas to
 * suspend, skip, or even panic the process. Fault injectors are set in shared
 * memory so they are accessible to all segment processes.
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
#include "cdb/cdbutil.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "postmaster/autovacuum.h"
#include "postmaster/bgwriter.h"
#include "storage/spin.h"
#include "storage/shmem.h"
#include "utils/faultinjector.h"
#include "utils/hsearch.h"
#include "miscadmin.h"

/*
 * internal include
 */
#include "faultinjector_warnings.h"

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

bool am_faulthandler = false;

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

static int FaultInjector_MarkEntryAsResume(
								FaultInjectorEntry_s	*entry);

static bool FaultInjector_RemoveHashEntry(
								const char* faultName);

static int FaultInjector_SetFaultInjection(FaultInjectorEntry_s *entry);

static FaultInjectorType_e FaultInjectorTypeStringToEnum(const char *faultType);

static DDLStatement_e FaultInjectorDDLStringToEnum(const char *ddlString);

/* Arrays to map between enum values and strings */
const char*
FaultInjectorTypeEnumToString[] = {
#define FI_TYPE(id, str) str,
#include "utils/faultinjector_lists.h"
#undef FI_TYPE
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

static FaultInjectorType_e
FaultInjectorTypeStringToEnum(const char* faultTypeString)
{
	FaultInjectorType_e	faultTypeEnum = FaultInjectorTypeMax;
	int	ii;
	
	for (ii=FaultInjectorTypeNotSpecified+1; ii < FaultInjectorTypeMax; ii++)
	{
		if (strcmp(FaultInjectorTypeEnumToString[ii], faultTypeString) == 0)
		{
			faultTypeEnum = ii;
			break;
		}
	}
	return faultTypeEnum;
}

static DDLStatement_e
FaultInjectorDDLStringToEnum(const char* ddlString)
{
	DDLStatement_e	ddlEnum = DDLMax;
	int	ii;
	
	for (ii=DDLNotSpecified; ii < DDLMax; ii++)
	{
		if (strcmp(FaultInjectorDDLEnumToString[ii], ddlString) == 0)
		{
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


void InjectFaultInit(void)
{
	warnings_init();
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

	if (strlen(faultName) >= sizeof(localEntry.faultName))
		elog(ERROR, "fault name too long: '%s'", faultName);
	if (strcmp(faultName, FaultInjectorNameAll) == 0)
		elog(ERROR, "invalid fault name '%s'", faultName);
	if (strlen(databaseName) >= NAMEDATALEN)
		elog(ERROR, "database name too long:'%s'", databaseName);
	if (strlen(tableName) >= NAMEDATALEN)
		elog(ERROR, "table name too long: '%s'", tableName);

	/*
	 * Auto-vacuum worker and launcher process, may run at unpredictable times
	 * while running tests. So, skip setting any faults for auto-vacuum
	 * launcher or worker. If anytime in future need to test these processes
	 * using fault injector framework, this restriction needs to be lifted and
	 * some other mechanism needs to be placed to avoid flaky failures.
	 */
	if (IsAutoVacuumLauncherProcess() ||
		(IsAutoVacuumWorkerProcess() &&
		 !(0 == strcmp("vacuum_update_dat_frozen_xid", faultName) ||
			 0 == strcmp("auto_vac_worker_before_do_autovacuum", faultName))))
		return FaultInjectorTypeNotSpecified;

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

		entryShared->numTimesTriggered++;

		if (entryShared->numTimesTriggered < entryShared->startOccurrence)
		{
			break;
		}

		/* Update the injection fault entry in hash table */
		entryShared->faultInjectorState = FaultInjectorStateTriggered;

		/* Mark fault injector to completed */
		if (entryShared->endOccurrence != INFINITE_END_OCCURRENCE &&
			entryShared->numTimesTriggered >= entryShared->endOccurrence)
			entryShared->faultInjectorState = FaultInjectorStateCompleted;

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
			
			pg_usleep(entryLocal->extraArg * 1000000L);
			break;

		case FaultInjectorTypeFatal:
			/*
			 * Sometimes Fatal is upgraded to Panic (e.g. when it is called in
			 * critical section or when it is called during QD prepare
			 * handling).  We should avoid core file generation for this
			 * scenario, just like what we do for the FaultInjectorTypePanic
			 * case.  Even FATAL is not upgraded to PANIC the process will quit
			 * soon, it does not affect subsequent code.
			 */
			AvoidCorefileGeneration();
			ereport(FATAL, 
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));
			break;

		case FaultInjectorTypePanic:
			AvoidCorefileGeneration();
			ereport(PANIC, 
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));	

			break;

		case FaultInjectorTypeError:
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

			for (ii=0;
				 ii < cnt && FaultInjector_LookupHashEntry(entryLocal->faultName);
				 ii++)
			{
				pg_usleep(1000000L); // sleep for 1 sec (1 sec * 3600 = 1 hour)
				CHECK_FOR_INTERRUPTS();
			}
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

		case FaultInjectorTypeResume:
			break;
			
		case FaultInjectorTypeSegv:
		{
			AvoidCorefileGeneration();
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

		default:
			
			ereport(LOG, 
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("unexpected error, fault triggered, fault name:'%s' fault type:'%s' ",
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));	
			
			Assert(0);
			break;
	}
		
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
	
	elog(DEBUG1, "FaultInjector_InsertHashEntry() entry_key:%s",
		 entry->faultName);
	
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
				(errmsg("cannot insert fault injection, no slots available"),
				 errdetail("Fault name:'%s' fault type:'%s'",
						   entry->faultName,
						   FaultInjectorTypeEnumToString[entry->faultInjectorType])));
		snprintf(entry->bufOutput, sizeof(entry->bufOutput), 
				 "could not insert fault injection, max slots:'%d' reached",
				 FAULTINJECTOR_MAX_SLOTS);
		
		goto exit;
	}

	entryLocal = FaultInjector_InsertHashEntry(entry->faultName, &exists);
		
	if (entryLocal == NULL) {
		FiLockRelease();
		status = STATUS_ERROR;
		ereport(WARNING,
				(errmsg("cannot insert fault injection entry into table, no memory"),
				 errdetail("Fault name:'%s' fault type:'%s'",
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
				(errmsg("cannot insert fault injection entry into table, entry already exists"),
				 errdetail("Fault name:'%s' fault type:'%s' ",
						   entry->faultName,
						   FaultInjectorTypeEnumToString[entry->faultInjectorType])));
		snprintf(entry->bufOutput, sizeof(entry->bufOutput), 
				 "could not insert fault injection, entry already exists");
		
		goto exit;
	}
		
	entryLocal->faultInjectorType = entry->faultInjectorType;
	strlcpy(entryLocal->faultName, entry->faultName, sizeof(entryLocal->faultName));

	entryLocal->extraArg = entry->extraArg;
	entryLocal->ddlStatement = entry->ddlStatement;
	
	entryLocal->startOccurrence = entry->startOccurrence;
	entryLocal->endOccurrence = entry->endOccurrence;

	entryLocal->numTimesTriggered = 0;
	strcpy(entryLocal->databaseName, entry->databaseName);
	strcpy(entryLocal->tableName, entry->tableName);
		
	entryLocal->faultInjectorState = FaultInjectorStateWaiting;

	faultInjectorShmem->faultInjectorSlots++;
		
	FiLockRelease();
	
	elog(DEBUG1, "FaultInjector_NewHashEntry(): '%s'", entry->faultName);
	
exit:
		
	return status;			
}

/*
 * update hash entry with state 
 */		
static int 
FaultInjector_MarkEntryAsResume(
							FaultInjectorEntry_s	*entry)
{
	
	FaultInjectorEntry_s	*entryLocal;
	int						status = STATUS_OK;

	Assert(entry->faultInjectorType == FaultInjectorTypeResume);

	FiLockAcquire();

	entryLocal = FaultInjector_LookupHashEntry(entry->faultName);
	
	if (entryLocal == NULL)
	{
		FiLockRelease();
		status = STATUS_ERROR;
		ereport(WARNING,
				(errmsg("cannot update fault injection hash entry with fault injection status, no entry found"),
				 errdetail("Fault name:'%s' fault type:'%s'",
						   entry->faultName,
						   FaultInjectorTypeEnumToString[entry->faultInjectorType])));
		goto exit;
	}

	if (entryLocal->faultInjectorType != FaultInjectorTypeSuspend)
	{
		FiLockRelease();
		ereport(ERROR, 
				(errcode(ERRCODE_FAULT_INJECT),
				 errmsg("only suspend fault can be resumed")));	
	}

	entryLocal->faultInjectorType = FaultInjectorTypeResume;
	
	FiLockRelease();
	
	ereport(DEBUG1,
			(errmsg("LOG(fault injector): update fault injection hash entry identifier:'%s' state:'%s'",
					entry->faultName,
					FaultInjectorStateEnumToString[entryLocal->faultInjectorState])));
	
exit:	
	
	return status;			
}

/*
 * Inject fault according to its type.
 */
static int
FaultInjector_SetFaultInjection(
						   FaultInjectorEntry_s	*entry)
{
	int		status = STATUS_OK;
	bool	isRemoved = FALSE;
	
	switch (entry->faultInjectorType) {
		case FaultInjectorTypeReset:
		{
			HASH_SEQ_STATUS			hash_status;
			FaultInjectorEntry_s	*entryLocal;
			
			if (strcmp(entry->faultName, FaultInjectorNameAll) == 0)
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
				
			if (isRemoved == FALSE)
				ereport(DEBUG1,
						(errmsg("LOG(fault injector): could not remove fault injection from hash identifier:'%s'",
								entry->faultName)));
			
			break;
		}

		case FaultInjectorTypeWaitUntilTriggered:
		{
			FaultInjectorEntry_s	*entryLocal;
			int retry_count = 3000; /* 10 minutes */

			while ((entryLocal = FaultInjector_LookupHashEntry(entry->faultName)) != NULL &&
				   entryLocal->faultInjectorState != FaultInjectorStateCompleted &&
				   entryLocal->numTimesTriggered - entryLocal->startOccurrence < entry->extraArg - 1)
			{
				pg_usleep(200000);  /* 0.2 sec */
				retry_count--;
				if (!retry_count)
				{
					ereport(ERROR,
							(errcode(ERRCODE_FAULT_INJECT),
							 errmsg("fault not triggered, fault name:'%s' fault type:'%s' ",
									entryLocal->faultName,
									FaultInjectorTypeEnumToString[entry->faultInjectorType]),
							 errdetail("Timed-out as 10 minutes max wait happens until triggered.")));
				}
			}

			if (entryLocal != NULL)
			{
				ereport(LOG,
						(errcode(ERRCODE_FAULT_INJECT),
						 errmsg("fault triggered %d times, fault name:'%s' fault type:'%s' ",
							entryLocal->numTimesTriggered,
							entryLocal->faultName,
							FaultInjectorTypeEnumToString[entry->faultInjectorType])));
				status = STATUS_OK;
			}
			else
			{
				ereport(ERROR,
						(errcode(ERRCODE_FAULT_INJECT),
						 errmsg("fault not set, fault name:'%s'  ",
								entry->faultName)));
			}
			break;
		}

		case FaultInjectorTypeStatus:
		{	
			FaultInjectorEntry_s	*entryLocal;
			int                     length;

			if (faultInjectorShmem->hash == NULL)
			{
				status = STATUS_ERROR;
				break;
			}
			length = snprintf(entry->bufOutput, sizeof(entry->bufOutput), "Success: ");

			entryLocal = FaultInjector_LookupHashEntry(entry->faultName);
			if (entryLocal)
			{
				length = snprintf(
					(entry->bufOutput + length),
					sizeof(entry->bufOutput) - length,
					"fault name:'%s' "
					"fault type:'%s' "
					"ddl statement:'%s' "
					"database name:'%s' "
					"table name:'%s' "
					"start occurrence:'%d' "
					"end occurrence:'%d' "
					"extra arg:'%d' "
					"fault injection state:'%s'  "
					"num times hit:'%d' \n",
					entryLocal->faultName,
					FaultInjectorTypeEnumToString[entryLocal->faultInjectorType],
					FaultInjectorDDLEnumToString[entryLocal->ddlStatement],
					entryLocal->databaseName,
					entryLocal->tableName,
					entryLocal->startOccurrence,
					entryLocal->endOccurrence,
					entryLocal->extraArg,
					FaultInjectorStateEnumToString[entryLocal->faultInjectorState],
					entryLocal->numTimesTriggered);
			}
			else
			{
				length = snprintf(entry->bufOutput, sizeof(entry->bufOutput),
								  "Failure: fault name:'%s' not set",
								  entry->faultName);
			}
			elog(LOG, "%s", entry->bufOutput);
			if (length > sizeof(entry->bufOutput))
				elog(LOG, "fault status truncated from %d to %zu characters",
					 length, sizeof(entry->bufOutput));
			break;
		}
		case FaultInjectorTypeResume:
		{
			ereport(LOG, 
					(errcode(ERRCODE_FAULT_INJECT),
					 errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							entry->faultName,
							FaultInjectorTypeEnumToString[entry->faultInjectorType])));	
			
			FaultInjector_MarkEntryAsResume(entry);
			
			break;
		}
		default: 
			
			status = FaultInjector_NewHashEntry(entry);
			break;
	}
	return status;
}

void
HandleFaultMessage(const char* msg)
{
	char name[NAMEDATALEN];
	char type[NAMEDATALEN];
	char ddl[NAMEDATALEN];
	char db[NAMEDATALEN];
	char table[NAMEDATALEN];
	int start;
	int end;
	int extra;
	char *result;
	int len;

	if (sscanf(msg, "faultname=%s type=%s ddl=%s db=%s table=%s "
			   "start=%d end=%d extra=%d",
			   name, type, ddl, db, table, &start, &end, &extra) != 8)
		elog(ERROR, "invalid fault message: %s", msg);
	/* The value '#' means not specified. */
	if (ddl[0] == '#')
		ddl[0] = '\0';
	if (db[0] == '#')
		db[0] = '\0';
	if (table[0] == '#')
		table[0] = '\0';

	result = InjectFault(name, type, ddl, db, table, start, end, extra);
	len = strlen(result);

	StringInfoData buf;
	pq_beginmessage(&buf, 'T');
	pq_sendint(&buf, Natts_fault_message_response, 2);

	pq_sendstring(&buf, "status");
	pq_sendint(&buf, 0, 4);		/* table oid */
	pq_sendint(&buf, Anum_fault_message_response_status, 2);		/* attnum */
	pq_sendint(&buf, TEXTOID, 4);		/* type oid */
	pq_sendint(&buf, -1, 2);	/* typlen */
	pq_sendint(&buf, -1, 4);		/* typmod */
	pq_sendint(&buf, 0, 2);		/* format code */
	pq_endmessage(&buf);

	/* Send a DataRow message */
	pq_beginmessage(&buf, 'D');
	pq_sendint(&buf, Natts_fault_message_response, 2);		/* # of columns */

	pq_sendint(&buf, len, 4);
	pq_sendbytes(&buf, result, len);
	pq_endmessage(&buf);
	EndCommand(GPCONN_TYPE_FAULT, DestRemote);
	pq_flush();
}

char *
InjectFault(char *faultName, char *type, char *ddlStatement, char *databaseName,
			char *tableName, int startOccurrence, int endOccurrence, int extraArg)
{
	StringInfo buf = makeStringInfo();
	FaultInjectorEntry_s faultEntry;

	elog(DEBUG1, "injecting fault: name %s, type %s, DDL %s, db %s, table %s, startOccurrence %d, endOccurrence %d, extraArg %d",
		 faultName, type, ddlStatement, databaseName, tableName,
		 startOccurrence, endOccurrence, extraArg );

	faultEntry.faultInjectorType = FaultInjectorTypeStringToEnum(type);
	faultEntry.ddlStatement = FaultInjectorDDLStringToEnum(ddlStatement);
	faultEntry.extraArg = extraArg;
	faultEntry.startOccurrence = startOccurrence;
	faultEntry.endOccurrence = endOccurrence;

	/*
	 * Validations:
	 *
	 */
	if (strlcpy(faultEntry.faultName, faultName, sizeof(faultEntry.faultName)) >=
		sizeof(faultEntry.faultName))
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("fault name too long: '%s'", faultName),
				 errdetail("Fault name should be no more than %d characters.",
						   FAULT_NAME_MAX_LENGTH-1)));

	if (faultEntry.faultInjectorType == FaultInjectorTypeMax)
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("could not recognize fault type '%s'", type)));

	/* Special fault name "all" is only used to reset all faults */
	if (faultEntry.faultInjectorType != FaultInjectorTypeReset &&
		strcmp(faultEntry.faultName, FaultInjectorNameAll) == 0)
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("invalid fault name '%s'", faultName)));

	if (faultEntry.faultInjectorType == FaultInjectorTypeSleep)
	{
		if (extraArg < 0 || extraArg > 7200)
			ereport(ERROR,
					(errcode(ERRCODE_PROTOCOL_VIOLATION),
					 errmsg("invalid sleep time, allowed range [0, 7200 sec]")));
	}

	if (faultEntry.ddlStatement == DDLMax)
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("could not recognize DDL statement")));

	if (strlcpy(faultEntry.databaseName, databaseName,
				sizeof(faultEntry.databaseName)) >=
		sizeof(faultEntry.databaseName))
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("database name too long: '%s'", databaseName),
				 errdetail("Database name should be no more than %d characters.",
						   NAMEDATALEN-1)));

	if (strlcpy(faultEntry.tableName, tableName, sizeof(faultEntry.tableName)) >=
		sizeof(faultEntry.tableName))
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("table name too long: '%s'", tableName),
				 errdetail("Table name should be no more than %d characters.",
						   NAMEDATALEN-1)));

	if (startOccurrence < 1 || startOccurrence > 1000)
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("invalid start occurrence number, allowed range [1, 1000]")));


	if (endOccurrence != INFINITE_END_OCCURRENCE && endOccurrence < startOccurrence)
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("invalid end occurrence number, allowed range [startOccurrence, ] or -1")));


	/*
	 * Warnings:
	 *
	 */
	emit_warnings(faultEntry);


	if (FaultInjector_SetFaultInjection(&faultEntry) == STATUS_OK)
	{
		if (faultEntry.faultInjectorType == FaultInjectorTypeStatus)
			appendStringInfo(buf, "%s", faultEntry.bufOutput);
		else
		{
			appendStringInfo(buf, "Success:");
			elog(LOG, "injected fault '%s' type '%s'", faultName, type);
		}
	}
	else
		appendStringInfo(buf, "Failure: %s", faultEntry.bufOutput);

	return buf->data;
}
#endif
