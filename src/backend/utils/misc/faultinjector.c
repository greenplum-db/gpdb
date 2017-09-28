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
	_("transaction_start_under_entry_db_singleton"),
		/* inject fault during transaction start with DistributedTransactionContext in ENTRY_DB_SINGLETON mode */
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
	_("workfile_hashjoin_failure"),
	  /* pretend that a query processed billions of rows  */
	_("executor_run_high_processed"),
	 /* inject fault before we close workfile in ExecHashJoinNewBatch */
	_("update_committed_eof_in_persistent_table"),
		/* inject fault before committed EOF is updated in gp_persistent_relation_node for Append Only segment files */
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
	_("vacuum_relation_open_relation_during_drop_phase"),
		/* inject fault during the open relation of the drop phase of vacuumRelation loop */
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
	_("interconnect_stop_ack_is_lost"),
		/* inject fault in interconnect to skip sending the stop ack */
	_("qe_got_snapshot_and_interconnect"),
		/* inject fault after qe got snapshot and interconnect*/
	_("fsync_counter"),
		/* inject fault to 'skip' in order to flush all buffers in BgBufferSync() */
	_("bg_buffer_sync_default_logic"),
		/* inject fault to count buffers fsync'ed by checkpoint process */
	_("finish_prepared_after_record_commit_prepared"),
		/* inject fault in FinishPreparedTransaction() after recording the commit prepared record */
	_("gang_created"),
		/* inject fault to report ERROR just after creating Gang */
	_("resgroup_assigned_on_master"),
		/* inject fault to report ERROR just after resource group is assigned on master */
	_("before_read_command"),
		/* inject fault before reading command */
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
