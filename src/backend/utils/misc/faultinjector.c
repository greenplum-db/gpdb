/*
 * faultinjector.c
 * Copyright 2009-2010 Greenplum Inc. All rights reserved.
 *
 * GP Fault Injector utility (gpfaultinjector python script) is used 
 * for Greenplum internal testing only. 
 * 
 * The utility inject faults (as defined by 'fault_type') on primary or mirror segment 
 *	at predefined 'fault_name. 
 * 
 * The utility is started on master host. Master host sends the fault injection request to specified segment. 
 *			It connects to postmaster on a segment.
 *			Postmaster spawns backend process that sets fault injection request into shared memory.
 *			Shared memory is accessible to all segment processes.
 *			Segment processes are checking shared memory to find if/when fault has to be injected.
 *
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
								FaultInjectorIdentifier_e identifier);

static FaultInjectorEntry_s* FaultInjector_InsertHashEntry(
								FaultInjectorIdentifier_e identifier, 
								bool	*exists);

static int FaultInjector_NewHashEntry(
								FaultInjectorEntry_s	*entry);

static int FaultInjector_UpdateHashEntry(
								FaultInjectorEntry_s	*entry);

static bool FaultInjector_RemoveHashEntry(
								FaultInjectorIdentifier_e identifier);

/*
 * NB: This list needs to be kept in sync with:
 * - FaultInjectorType_e
 * - the help message in clsInjectFault.py
 */
const char*
FaultInjectorTypeEnumToString[] = {
	_(""), /* not specified */
	_("sleep"),
	_("fault"),
	_("fatal"),
	_("panic"),
	_("error"),
	_("infinite_loop"),
	_("data_corruption"),
	_("suspend"),
	_("resume"),
	_("skip"),
	_("memory_full"),
	_("reset"),
	_("status"),
	_("segv"),
	_("interrupt"),
	_("finish_pending"),
	_("checkpoint_and_panic"),
	_("not recognized"),
};

const char*
FaultInjectorIdentifierEnumToString[] = {
	_(""),		
		/* not specified */
	_("all"),  
		/* reset or display all injected faults */
	_("postmaster"),
		/* inject fault when new connection is accepted in postmaster */
	_("pg_control"),
		/* inject fault when pg_control file is written */
	_("pg_xlog"),
		/* inject fault when files in pg_xlog directory are written */
	_("start_prepare"),
		/* inject fault during start prepare */
	_("fault_before_pending_delete_relation_entry"),
		/* inject fault after adding entry to persistent relation table in CP state but before adding to Pending delete list */
	_("fault_before_pending_delete_database_entry"),
		/* inject fault after adding entry to persistent database table in CP state but before adding to Pending delete list */
	_("fault_before_pending_delete_tablespace_entry"),
		/* inject fault after adding entry to persistent tablespace table in CP state but before adding to Pending delete list */
	_("fault_before_pending_delete_filespace_entry"),
		/* inject fault after adding entry to persistent filespace table in CP state but before adding to Pending delete list */
	_("filerep_consumer"),
		/* 
		 * inject fault before data are processed
		 *		*) file operation is issued to file system (if mirror)
		 *		*) file operation performed on mirror is acknowledged to backend processes (if primary)
		 */
	_("filerep_consumer_verification"),
		/* inject fault before ack verification data are consumed on primary */
	_("filerep_change_tracking_compacting"),
		/* Ashwin - inject fault during compacting change tracking */
	_("filerep_sender"),
		/* inject fault before data are sent to network */
	_("filerep_receiver"),
		/* 
		 * inject fault after data are received from the network and 
		 * before data are made available for consuming 
		 */
	_("filerep_flush"),
		/* inject fault before fsync is issued to file system */
	_("filerep_resync"),
		/* inject fault while InResync when first relations is inserted to be resynced */
	_("filerep_resync_in_progress"),
		/* inject fault while InResync when more then 10 relations in progress */
	_("filerep_resync_worker"),
		/* inject fault after write to mirror while all locks are still hold */
	_("filerep_resync_worker_read"),
		/* inject fault on read required for resync by resync worker process */
	_("filerep_transition_to_resync"),
		/* inject fault during transition to InResync before objects are re-created on mirror */
	_("filerep_transition_to_resync_mark_recreate"),
		/* inject fault during transition to InResync before objects are marked re-created */
	_("filerep_transition_to_resync_mark_completed"),
		/* inject fault during transition to InResync before transition is marked completed */
	_("filerep_transition_to_sync_begin"),
		/* inject fault before transition to InSync begin */
	_("filerep_transition_to_sync"),
		/* inject fault during transition to InSync */
	_("filerep_transition_to_sync_before_checkpoint"),
		/* inject fault during transition to InSync before checkpoint is taken */
	_("filerep_transition_to_sync_mark_completed"),
		/* inject fault during transition to InSync before transition is marked completed */
	_("filerep_transition_to_change_tracking"),
		/* inject fault during transition to Change Tracking */
	_("fileRep_is_operation_completed"),
		/* inject fault in FileRep Is Operation completed function */
	_("filerep_immediate_shutdown_request"),
		/* inject fault just before sending SIGQUIT to child flerep processes */
	_("checkpoint"),
		/* inject fault before checkpoint is taken */
	_("change_tracking_compacting_report"),
		/* report if compacting is in progress */
	_("change_tracking_disable"),
		/* inject fault during fsync to Change Tracking log */
	_("transaction_abort_after_distributed_prepared"),
		/* inject fault after transaction is prepared */
	_("transaction_commit_pass1_from_create_pending_to_created"),
		/* inject fault after persistent state change is permanently stored during first pass */
	_("transaction_commit_pass1_from_drop_in_memory_to_drop_pending"),
		/* inject fault after persistent state change is permanently stored during first pass */
	_("transaction_commit_pass1_from_aborting_create_needed_to_aborting_create"),
		/* inject fault after persistent state change is permanently stored during first pass */
	_("transaction_abort_pass1_from_create_pending_to_aborting_create"),
		/* inject fault after persistent state change is permanently stored during first pass */
	_("transaction_abort_pass1_from_aborting_create_needed_to_aborting_create"),
		/* inject fault after persistent state change is permanently stored during first pass */
	_("transaction_commit_pass2_from_drop_in_memory_to_drop_pending"),
		/* inject fault after physical drop and before final persistent state change is permanently stored during second pass */
	_("transaction_commit_pass2_from_aborting_create_needed_to_aborting_create"),
		/* inject fault after physical drop and before final persistent state change is permanently stored during second pass */
	_("transaction_abort_pass2_from_create_pending_to_aborting_create"),
		/* inject fault after physical drop and before final persistent state change is permanently stored during second pass */
	_("transaction_abort_pass2_from_aborting_create_needed_to_aborting_create"),
		/* inject fault after physical drop and before final persistent state change is permanently stored during second pass */
	_("finish_prepared_transaction_commit_pass1_from_create_pending_to_created"),
		/* inject fault after persistent state change is permanently stored during first pass */
	_("finish_prepared_transaction_commit_pass2_from_create_pending_to_created"),
		/* inject fault after physical drop and before final persistent state change is permanently stored during second pass */
	_("finish_prepared_transaction_abort_pass1_from_create_pending_to_aborting_create"),
		/* inject fault after persistent state change is permanently stored during first pass */
	_("finish_prepared_transaction_abort_pass2_from_create_pending_to_aborting_create"),
		/* inject fault after physical drop and before final persistent state change is permanently stored during second pass */
	_("finish_prepared_transaction_commit_pass1_from_drop_in_memory_to_drop_pending"),
		/* inject fault after persistent state change is permanently stored during first pass */
	_("finish_prepared_transaction_commit_pass2_from_drop_in_memory_to_drop_pending"),
		/* inject fault after physical drop and before final persistent state change is permanently stored during second pass */
	_("finish_prepared_transaction_commit_pass1_aborting_create_needed"),
		/* inject fault after persistent state change is permanently stored during first pass */
	_("finish_prepared_transaction_commit_pass2_aborting_create_needed"),
		/* inject fault after physical drop and before final persistent state change is permanently stored during second pass */
	_("finish_prepared_transaction_abort_pass1_aborting_create_needed"),
		/* inject fault after persistent state change is permanently stored during first pass */
	_("finish_prepared_transaction_abort_pass2_aborting_create_needed"),
		/* inject fault after physical drop and before final persistent state change is permanently stored during second pass */
	_("filerep_verification"),
	    /* inject fault to start verification */
	_("twophase_transaction_commit_prepared"),
		/* inject fault before transaction commit is recorded in xlog */
	_("twophase_transaction_abort_prepared"),
		 /* inject fault before transaction abort is recorded in xlog */
	_("dtm_broadcast_prepare"),
		/* inject fault after prepare broadcast */
	_("dtm_broadcast_commit_prepared"),
		/* inject fault after commit broadcast */
	_("dtm_broadcast_abort_prepared"),
		/* inject fault after abort broadcast */
	_("dtm_xlog_distributed_commit"),
		/* inject fault after distributed commit was inserted in xlog */
	_("dtm_init"),
		/* inject fault before initializing dtm */
        _("end_prepare_two_phase_sleep"),
	        /* inject sleep after creation of two phase file */
	_("segment_transition_request"),
    	/* inject fault after segment receives state transition request */
	_("segment_probe_response"),
		/* inject fault after segment is probed by FTS */
	_("SubtransactionFlushToFile"),
		/* inject fault while writing subxids to file */
	_("SubtransactionReadFromFile"),
		/* inject fault while reading subxids from file */
	_("SubtransactionRelease"),
		/* inject fault before sub-transaction commit is recorded in xlog */
	_("SubtransactionRollback"),
		/* inject fault before sub-transaction abort is recorded in xlog */
	_("local_tm_record_transaction_commit"),
		/* inject fault after recording transaction commit for local transaction  */
	_("malloc_failure"),
		/* inject fault to simulate memory allocation failure */
	_("transaction_abort_failure"),
		/* inject fault to simulate transaction abort failure  */
	_("workfile_creation_failure"),
	  /* inject fault to simulate workfile creation failure  */
	_("workfile_write_failure"),
	  /* inject fault to simulate workfile write failure  */
	_("update_committed_eof_in_persistent_table"),
		/* inject fault before committed EOF is updated in gp_persistent_relation_node for Append Only segment files */
	_("exec_simple_query_end_command"),
		/* inject fault before EndCommand in exec_simple_query */
	_("multi_exec_hash_large_vmem"),
		/* large palloc inside MultiExecHash to attempt to exceed vmem limit */
	_("execsort_before_sorting"),
		/* inject fault in ExecSort before doing the actual sort */
	_("execsort_mksort_mergeruns"),
		/* inject fault in MKSort during the mergeruns phase */
	_("execshare_input_next"),
		/* inject fault after shared input scan retrieved a tuple */
	_("base_backup_post_create_checkpoint"),
		/* inject fault after creation of checkpoint when basebackup requested */
	_("compaction_before_segmentfile_drop"),
		/* inject fault after compaction, but before the drop of the
		 * segment file */
	_("compaction_before_cleanup_phase"),
		/* inject fault after compaction and drop, but before
		 * the cleanup phase for a relation */
	_("appendonly_insert"),
		/* inject fault before an append-only insert */
	_("appendonly_delete"),
		/* inject fault before an append-only delete */
	_("appendonly_update"),
		/* inject fault before an append-only update */
	_("reindex_db"),
		/* inject fault while reindex db is in progress */
	_("reindex_relation"),
		/* inject fault while reindex relation is in progress */
	_("fault_during_exec_dynamic_table_scan"),
		/* inject fault during scanning of a partition */
	_("fault_in_background_writer_main"),
		/* inject fault at the beginning of rxThreadFunc */
	_("cdb_copy_start_after_dispatch"),
		/* inject fault in cdbCopyStart after dispatch */
	_("repair_frag_end"),
		/* inject fault at the end of repair_frag */
	_("vacuum_full_before_truncate"),
		/* inject fault before truncate in vacuum full */
	_("vacuum_full_after_truncate"),
		/* inject fault after truncate in vacuum full */
	_("vacuum_relation_end_of_first_round"),
		/* inject fault at the end of first round of vacuumRelation loop */
	_("rebuild_pt_db"),
		/* inject fault while rebuilding persistent tables (for each db) */
	_("procarray_add"),
		/* inject fault while adding PGPROC to procarray */
	_("exec_hashjoin_new_batch"),
		/* inject fault before switching to a new batch in Hash Join */
	_("fts_wait_for_shutdown"),
		/* pause FTS process before committing changes, until shutdown */
	_("runaway_cleanup"),
		/* inject fault before cleaning up a runaway query */		
	_("opt_task_allocate_string_buffer"),
		/* inject fault while allocating string buffer */
	_("opt_relcache_translator_catalog_access"),
		/* inject fault while translating relcache entries */
	_("send_qe_details_init_backend"),
		/* inject fault before sending QE details during backend initialization */
	_("process_startup_packet"),
		/* inject fault in ProcessStartupPacket() */
	_("quickdie"),
		/* inject fault in quickdie*/
	_("after_one_slice_dispatched"),
		/* inject fault in cdbdisp_dispatchX*/
	_("not recognized"),
};

const char*
FaultInjectorDDLEnumToString[] = {
	_(""),		/* not specified */
	_("create_database"),
	_("drop_database"),
	_("create_table"),
	_("drop_table"),
	_("create_index"),
	_("alter_index"),
	_("reindex"),
	_("drop_index"),
	_("create_filespaces"),
	_("drop_filespaces"),
	_("create_tablespaces"),
	_("drop_tablespaces"),
	_("truncate"),
	_("vacuum"),
	_("not recognized"),
};

const char*
FaultInjectorStateEnumToString[] = {
	_("not initialized"),
	_("set"),
	_("triggered"),
	_("completed"),
	_("failed"),
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
	hash_ctl.keysize = sizeof(int32);
	hash_ctl.entrysize = sizeof(FaultInjectorEntry_s);
	hash_ctl.hash = int32_hash;
	
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
	
	FaultInjectorEntry_s   *entryShared, localEntry,
						   *entryLocal = &localEntry;
	char					databaseNameLocal[NAMEDATALEN];
	char					tableNameLocal[NAMEDATALEN];
	int						ii = 0;
	int cnt = 3600;

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
		return FALSE;

	getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);

	FiLockAcquire();

	entryShared = FaultInjector_LookupHashEntry(identifier);

	if (entryShared != NULL)
		memcpy(entryLocal, entryShared, sizeof(FaultInjectorEntry_s));

	FiLockRelease();
	
	/* Verify if fault injection is set */
	
	if (entryShared == NULL) 
		/* fault injection is not set */
		return FALSE;
	
	if (entryLocal->ddlStatement != ddlStatement) 
		/* fault injection is not set for the specified DDL */
		return FALSE;
	
	snprintf(databaseNameLocal, sizeof(databaseNameLocal), "%s", databaseName);
	
	if (strcmp(entryLocal->databaseName, databaseNameLocal) != 0) 
		/* fault injection is not set for the specified database name */
		return FALSE;
	
	snprintf(tableNameLocal, sizeof(tableNameLocal), "%s", tableName);

	if (strcmp(entryLocal->tableName, tableNameLocal) != 0) 
		/* fault injection is not set for the specified table name */
		return FALSE;
	
	if (entryLocal->faultInjectorState == FaultInjectorStateTriggered ||
		entryLocal->faultInjectorState == FaultInjectorStateCompleted ||
		entryLocal->faultInjectorState == FaultInjectorStateFailed) {
		/* fault injection was already executed */
		return FALSE;
	}

	/* Update the injection fault entry in hash table */
	if (entryLocal->occurrence != FILEREP_UNDEFINED)
	{
		if (entryLocal->occurrence > 1) 
		{
			entryLocal->occurrence--;
			FaultInjector_UpdateHashEntry(entryLocal);
			return FALSE;
		}

		entryLocal->faultInjectorState = FaultInjectorStateTriggered;
		FaultInjector_UpdateHashEntry(entryLocal);
	}

	/* Inject fault */
	
	switch (entryLocal->faultInjectorType) {
		case FaultInjectorTypeNotSpecified:
			
			break;
		case FaultInjectorTypeSleep:
			ereport(LOG, 
					(errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							FaultInjectorIdentifierEnumToString[entryLocal->faultInjectorIdentifier],
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
					(errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							FaultInjectorIdentifierEnumToString[entryLocal->faultInjectorIdentifier],
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
					(errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							FaultInjectorIdentifierEnumToString[entryLocal->faultInjectorIdentifier],
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
					(errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							FaultInjectorIdentifierEnumToString[entryLocal->faultInjectorIdentifier],
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
					(errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							FaultInjectorIdentifierEnumToString[entryLocal->faultInjectorIdentifier],
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));	
			break;
		case FaultInjectorTypeInfiniteLoop:
			ereport(LOG, 
					(errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							FaultInjectorIdentifierEnumToString[entryLocal->faultInjectorIdentifier],
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
					(errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							FaultInjectorIdentifierEnumToString[entryLocal->faultInjectorIdentifier],
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));							
			break;
			
		case FaultInjectorTypeSuspend:
		{
			FaultInjectorEntry_s	*entry;
			
			ereport(LOG, 
					(errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							FaultInjectorIdentifierEnumToString[entryLocal->faultInjectorIdentifier],
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));	
			
			while ((entry = FaultInjector_LookupHashEntry(entryLocal->faultInjectorIdentifier)) != NULL &&
				   entry->faultInjectorType != FaultInjectorTypeResume)
			{
				pg_usleep(1000000L);  // 1 sec
			}

			if (entry != NULL)
			{
				ereport(LOG, 
					(errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							FaultInjectorIdentifierEnumToString[entryLocal->faultInjectorIdentifier],
							FaultInjectorTypeEnumToString[entry->faultInjectorType])));	
			}
			else
			{
				ereport(LOG, 
						(errmsg("fault 'NULL', fault name:'%s'  ",
								FaultInjectorIdentifierEnumToString[entryLocal->faultInjectorIdentifier])));				

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
					(errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							FaultInjectorIdentifierEnumToString[entryLocal->faultInjectorIdentifier],
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));							
			break;
			
		case FaultInjectorTypeMemoryFull:
		{
			char	*buffer = NULL;
			
			ereport(LOG, 
					(errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							FaultInjectorIdentifierEnumToString[entryLocal->faultInjectorIdentifier],
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
					(errmsg("unexpected error, fault triggered, fault name:'%s' fault type:'%s' ",
							FaultInjectorIdentifierEnumToString[entryLocal->faultInjectorIdentifier],
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
					(errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							FaultInjectorIdentifierEnumToString[entryLocal->faultInjectorIdentifier],
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));

			InterruptPending = true;
			QueryCancelPending = true;
			break;
		}

		case FaultInjectorTypeFinishPending:
		{
			ereport(LOG,
					(errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							FaultInjectorIdentifierEnumToString[entryLocal->faultInjectorIdentifier],
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
					(errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							FaultInjectorIdentifierEnumToString[entryLocal->faultInjectorIdentifier],
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType])));
			break;
		}

		default:
			
			ereport(LOG, 
					(errmsg("unexpected error, fault triggered, fault name:'%s' fault type:'%s' ",
							FaultInjectorIdentifierEnumToString[entryLocal->faultInjectorIdentifier],
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
							  FaultInjectorIdentifier_e identifier)
{
	FaultInjectorEntry_s	*entry;
	
	Assert(faultInjectorShmem->hash != NULL);
	
	entry = (FaultInjectorEntry_s *) hash_search(
												  faultInjectorShmem->hash, 
												  (void *) &identifier, // key 
												  HASH_FIND, 
												  NULL);
	
	if (entry == NULL) {
		ereport(DEBUG5,
				(errmsg("FaultInjector_LookupHashEntry() could not find fault injection hash entry identifier:'%d' ",
						identifier)));
	} 
	
	return entry;
}

/*
 * insert fault injection in hash table 
 */ 
static FaultInjectorEntry_s*
FaultInjector_InsertHashEntry(
							FaultInjectorIdentifier_e identifier, 
							bool	*exists)
{
	
	bool					foundPtr;
	FaultInjectorEntry_s	*entry;

	Assert(faultInjectorShmem->hash != NULL);
	
	entry = (FaultInjectorEntry_s *) hash_search(
												  faultInjectorShmem->hash, 
												  (void *) &identifier, // key
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
							  FaultInjectorIdentifier_e identifier)
{	
	
	FaultInjectorEntry_s	*entry;
	bool					isRemoved = FALSE;
	
	Assert(faultInjectorShmem->hash != NULL);
	
	entry = (FaultInjectorEntry_s *) hash_search(
												  faultInjectorShmem->hash, 
												  (void *) &identifier, // key
												  HASH_REMOVE, 
												  NULL);
	
	if (entry) 
	{
		ereport(LOG, 
				(errmsg("fault removed, fault name:'%s' fault type:'%s' ",
						FaultInjectorIdentifierEnumToString[entry->faultInjectorIdentifier],
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
						FaultInjectorIdentifierEnumToString[entry->faultInjectorIdentifier],
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

			case SendQEDetailsInitBackend:

				break;
			default:
				
				FiLockRelease();
				status = STATUS_ERROR;
				ereport(WARNING,
						(errmsg("could not insert fault injection, fault type not supported"
								"fault name:'%s' fault type:'%s' ",
								FaultInjectorIdentifierEnumToString[entry->faultInjectorIdentifier],
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
		case StartPrepareTx:
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
		case TransactionCommitPass1FromCreatePendingToCreated:
		case TransactionCommitPass1FromDropInMemoryToDropPending:
		case TransactionCommitPass1FromAbortingCreateNeededToAbortingCreate:
		case TransactionAbortPass1FromCreatePendingToAbortingCreate:
		case TransactionAbortPass1FromAbortingCreateNeededToAbortingCreate:
		case TransactionCommitPass2FromDropInMemoryToDropPending:
		case TransactionCommitPass2FromAbortingCreateNeededToAbortingCreate:
		case TransactionAbortPass2FromCreatePendingToAbortingCreate:
		case TransactionAbortPass2FromAbortingCreateNeededToAbortingCreate:
			
		case FinishPreparedTransactionCommitPass1FromCreatePendingToCreated:
		case FinishPreparedTransactionCommitPass2FromCreatePendingToCreated:
			
		case FinishPreparedTransactionCommitPass1FromDropInMemoryToDropPending:
		case FinishPreparedTransactionCommitPass2FromDropInMemoryToDropPending:
			
		case FinishPreparedTransactionCommitPass1AbortingCreateNeeded:
		case FinishPreparedTransactionCommitPass2AbortingCreateNeeded:
			
		case FinishPreparedTransactionAbortPass1FromCreatePendingToAbortingCreate:
//		case FinishPreparedTransactionAbortPass2FromCreatePendingToAbortingCreate:
			
		case FinishPreparedTransactionAbortPass1AbortingCreateNeeded:
		case FinishPreparedTransactionAbortPass2AbortingCreateNeeded:
		case TwoPhaseTransactionCommitPrepared:
		case TwoPhaseTransactionAbortPrepared:
		
//		case SubtransactionFlushToFile:
//		case SubtransactionReadFromFile:
//		case SubtransactionRelease:
//		case SubtransactionRollback:
		/* Ashwin */
		case FileRepChangeTrackingCompacting:

		/* We do not use vmem on master. Therefore, we only attempt large palloc on segments. */
		case MultiExecHashLargeVmem:
		case FaultInBackgroundWriterMain:
		case SendQEDetailsInitBackend:
			
			if (fileRepRole != FileRepPrimaryRole)
			{
				FiLockRelease();
				status = STATUS_ERROR;
				ereport(WARNING,
						(errmsg("could not insert fault injection entry into table, segment not in primary role"
								"fault name:'%s' fault type:'%s' ",
								FaultInjectorIdentifierEnumToString[entry->faultInjectorIdentifier],
								FaultInjectorTypeEnumToString[entry->faultInjectorType])));
				snprintf(entry->bufOutput, sizeof(entry->bufOutput), 
						 "could not insert fault injection, segment not in primary role");
				
				goto exit;
			}			
			break;
		
		case FaultBeforePendingDeleteRelationEntry:
		case FaultBeforePendingDeleteDatabaseEntry:
		case FaultBeforePendingDeleteTablespaceEntry:
		case FaultBeforePendingDeleteFilespaceEntry:	
		case FileRepConsumer:
		case FileRepSender:
		case FileRepReceiver:
		case FileRepFlush:
		case Postmaster:
		case PgControl:
		case PgXlog:
		case SegmentTransitionRequest:
		case SegmentProbeResponse:
			
			if (fileRepRole != FileRepPrimaryRole && fileRepRole != FileRepMirrorRole)
			{
				FiLockRelease();
				status = STATUS_ERROR;
				ereport(WARNING,
						(errmsg("could not insert fault injection entry into table, "
								"segment not in primary or mirror role, "
								"fault name:'%s' fault type:'%s' ",
								FaultInjectorIdentifierEnumToString[entry->faultInjectorIdentifier],
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
		case OptTaskAllocateStringBuffer:
		case OptRelcacheTranslatorCatalogAccess:
			
			if (fileRepRole != FileRepNoRoleConfigured)
			{
				FiLockRelease();
				status = STATUS_ERROR;
				ereport(WARNING,
						(errmsg("could not insert fault injection entry into table, "
								"segment not in master role, "
								"fault name:'%s' fault type:'%s' ",
								FaultInjectorIdentifierEnumToString[entry->faultInjectorIdentifier],
								FaultInjectorTypeEnumToString[entry->faultInjectorType])));
				snprintf(entry->bufOutput, sizeof(entry->bufOutput), 
						 "could not insert fault injection, segment not in master role");
				
				goto exit;
			}			
			break;
			
		case LocalTmRecordTransactionCommit:
		case Checkpoint:
		case AbortTransactionFail:
		case WorkfileCreationFail:
		case WorkfileWriteFail:
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
								FaultInjectorIdentifierEnumToString[entry->faultInjectorIdentifier],
								FaultInjectorTypeEnumToString[entry->faultInjectorType])));
				snprintf(entry->bufOutput, sizeof(entry->bufOutput), 
						 "could not insert fault injection, segment not in primary or master role");
				
				goto exit;
			}			
			break;
			
		default:
			break;
	}
	entryLocal = FaultInjector_InsertHashEntry(entry->faultInjectorIdentifier, &exists);
		
	if (entryLocal == NULL) {
		FiLockRelease();
		status = STATUS_ERROR;
		ereport(WARNING,
				(errmsg("could not insert fault injection entry into table, no memory, "
						"fault name:'%s' fault type:'%s' ",
						FaultInjectorIdentifierEnumToString[entry->faultInjectorIdentifier],
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
						FaultInjectorIdentifierEnumToString[entry->faultInjectorIdentifier],
						FaultInjectorTypeEnumToString[entry->faultInjectorType])));
		snprintf(entry->bufOutput, sizeof(entry->bufOutput), 
				 "could not insert fault injection, entry already exists");
		
		goto exit;
	}
		
	entryLocal->faultInjectorType = entry->faultInjectorType;
	
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
	strcpy(entryLocal->databaseName, entry->databaseName);
	strcpy(entryLocal->tableName, entry->tableName);
		
	entryLocal->faultInjectorState = FaultInjectorStateWaiting;

	faultInjectorShmem->faultInjectorSlots++;
		
	FiLockRelease();
	
	elog(DEBUG1, "FaultInjector_NewHashEntry() identifier:'%s'", 
		 FaultInjectorIdentifierEnumToString[entry->faultInjectorIdentifier]);
	
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
	bool					exists;
	int						status = STATUS_OK;

	FiLockAcquire();

	entryLocal = FaultInjector_InsertHashEntry(entry->faultInjectorIdentifier, &exists);
	
	/* entry should be found since fault has not been injected yet */			
	Assert(entryLocal != NULL);
	
	if (!exists) {
		FiLockRelease();
		status = STATUS_ERROR;
		ereport(WARNING,
				(errmsg("could not update fault injection hash entry with fault injection status, "
						"no entry found, "
						"fault name:'%s' fault type:'%s' ",
						FaultInjectorIdentifierEnumToString[entry->faultInjectorIdentifier],
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
					FaultInjectorIdentifierEnumToString[entry->faultInjectorIdentifier], 
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
					isRemoved = FaultInjector_RemoveHashEntry(entryLocal->faultInjectorIdentifier);
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
				isRemoved = FaultInjector_RemoveHashEntry(entry->faultInjectorIdentifier);
				if (isRemoved == TRUE) {
					faultInjectorShmem->faultInjectorSlots--;
				}
				FiLockRelease();
			}
				
			if (isRemoved == FALSE) {
				ereport(DEBUG1,
						(errmsg("LOG(fault injector): could not remove fault injection from hash"
								"identifier:'%s' ",
								FaultInjectorIdentifierEnumToString[entry->faultInjectorIdentifier])));
			}			
			
			break;
		}
		case FaultInjectorTypeStatus:
		{	
			HASH_SEQ_STATUS			hash_status;
			FaultInjectorEntry_s	*entryLocal;
			bool					found = FALSE;
			
			if (faultInjectorShmem->hash == NULL) {
				status = STATUS_ERROR;
				break;
			} 
			snprintf(entry->bufOutput, sizeof(entry->bufOutput), "Success: ");
			
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
							"fault injection state:'%s' ",
							FaultInjectorIdentifierEnumToString[entryLocal->faultInjectorIdentifier],
							FaultInjectorTypeEnumToString[entryLocal->faultInjectorType],
							FaultInjectorDDLEnumToString[entryLocal->ddlStatement],
							entryLocal->databaseName,
							entryLocal->tableName,
							entryLocal->occurrence,
							entryLocal->sleepTime,
							FaultInjectorStateEnumToString[entryLocal->faultInjectorState])));
				
				if (entry->faultInjectorIdentifier == entryLocal->faultInjectorIdentifier ||
					entry->faultInjectorIdentifier == FaultInjectorIdAll) {
						snprintf(entry->bufOutput, sizeof(entry->bufOutput), 
								 "%s \n"
								 "fault name:'%s' "
								 "fault type:'%s' "
								 "ddl statement:'%s' "
								 "database name:'%s' "
								 "table name:'%s' "
								 "occurrence:'%d' "
								 "sleep time:'%d' "
								 "fault injection state:'%s' ",
								 entry->bufOutput,
								 FaultInjectorIdentifierEnumToString[entryLocal->faultInjectorIdentifier],
								 FaultInjectorTypeEnumToString[entryLocal->faultInjectorType],
								 FaultInjectorDDLEnumToString[entryLocal->ddlStatement],
								 entryLocal->databaseName,
								 entryLocal->tableName,
								 entryLocal->occurrence,
								 entryLocal->sleepTime,
								 FaultInjectorStateEnumToString[entryLocal->faultInjectorState]);		
						found = TRUE;
				}
			}
			if (found == FALSE) {
				snprintf(entry->bufOutput, sizeof(entry->bufOutput), "Failure: "
						 "fault name:'%s' not set",
						 FaultInjectorIdentifierEnumToString[entry->faultInjectorIdentifier]);
			}
			break;
		}
		case FaultInjectorTypeResume:
			ereport(LOG, 
					(errmsg("fault triggered, fault name:'%s' fault type:'%s' ",
							FaultInjectorIdentifierEnumToString[entry->faultInjectorIdentifier],
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
							  FaultInjectorIdentifier_e identifier)
{
	FaultInjectorEntry_s	*entry = NULL;
	bool					isCompleted = FALSE;
	bool					retval = FALSE;
	bool					isRemoved;
		
	FiLockAcquire();
		
	entry = FaultInjector_LookupHashEntry(identifier);
		
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
			isRemoved = FaultInjector_RemoveHashEntry(identifier);
			
			if (isRemoved == FALSE) {
				ereport(DEBUG1,
						(errmsg("LOG(fault injector): could not remove fault injection from hash"
								"identifier:'%s' ",
								FaultInjectorIdentifierEnumToString[identifier])));
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
						FaultInjectorIdentifierEnumToString[identifier],
						FaultInjectorTypeEnumToString[entry->faultInjectorType])));
	}
	return isCompleted;
}
#endif
