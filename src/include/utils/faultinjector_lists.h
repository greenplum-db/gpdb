/*
 * faultinjector_lists.h
 *
 * List of fault injector identifiers, or "fault points", as well as fault
 * injector states and some other things. These are listed using C
 * preprocessor macros. To use, you must define the appropriate FI_* macros
 * before #including this file.
 *
 * For example, to get an array of all the identifier strings, do:
 *
 * const char *FaultInject_Strings[] = {
 * #define FI_IDENT(id, str) str,
 * #include "utils/faultinjector_lists.h"
 * #undef FI_IDENT
 * };
 *
 *
 * To add a new fault injection point, simply add a new FI_IDENT line to the
 * list below.
 *
 *
 * Copyright 2009-2010, Greenplum Inc. All rights reserved.
 * Copyright (c) 2017-Present Pivotal Software, Inc.
 */

/* there is deliberately not an #ifndef FAULTINJECTOR_LISTS_H here */

/*
 * Fault injection points.
 */
#ifdef FI_IDENT
FI_IDENT(FaultInjectorIdNotSpecified = 0, "")
/*
 * affects all faults injected
 *		*) remove all faults injected from the segment
 *		*) display status for all faults injected
 */
FI_IDENT(FaultInjectorIdAll, "all")
/* inject fault when new connection is accepted in postmaster */
FI_IDENT(Postmaster, "postmaster")
/* inject fault when pg_control file is written */
FI_IDENT(PgControl, "pg_control")
/* inject fault when files in pg_xlog directory are written */
FI_IDENT(PgXlog, "pg_xlog")
/* inject fault during start prepare */
FI_IDENT(StartPrepareTx, "start_prepare")
/* inject fault before checkpoint is taken */
FI_IDENT(Checkpoint, "checkpoint")
FI_IDENT(TransactionStartUnderEntryDbSingleton, "transaction_start_under_entry_db_singleton")
/* inject fault after transaction is prepared */
FI_IDENT(TransactionAbortAfterDistributedPrepared, "transaction_abort_after_distributed_prepared")
/* inject fault before transaction commit is recorded in xlog */
FI_IDENT(OnePhaseTransactionCommit, "onephase_transaction_commit")
/* inject fault before transaction commit is recorded in xlog (trigger filerep verification)*/
FI_IDENT(TwoPhaseTransactionCommitPrepared, "twophase_transaction_commit_prepared")
/* inject fault before transaction abort is recorded in xlog */
FI_IDENT(TwoPhaseTransactionAbortPrepared, "twophase_transaction_abort_prepared")
/* inject fault after prepare broadcast */
FI_IDENT(DtmBroadcastPrepare, "dtm_broadcast_prepare")
/* inject fault after commit broadcast */
FI_IDENT(DtmBroadcastCommitPrepared, "dtm_broadcast_commit_prepared")
/* inject fault after abort broadcast */
FI_IDENT(DtmBroadcastAbortPrepared, "dtm_broadcast_abort_prepared")
/* inject fault after distributed commit was inserted in xlog */
FI_IDENT(DtmXLogDistributedCommit, "dtm_xlog_distributed_commit")
/* inject fault before initializing dtm */
FI_IDENT(DtmInit, "dtm_init")
/* inject sleep after creation of two phase file */
FI_IDENT(EndPreparedTwoPhaseSleep, "end_prepare_two_phase_sleep")
/* inject fault after segment receives state transition request (sleep after creating two phase files) */
FI_IDENT(SegmentTransitionRequest, "segment_transition_request")
/* inject fault after segment is probed by FTS */
FI_IDENT(SegmentProbeResponse, "segment_probe_response")
/* inject fault after recording transaction commit for local transaction  */
FI_IDENT(LocalTmRecordTransactionCommit, "local_tm_record_transaction_commit")
/* inject fault to simulate memory allocation failure */
FI_IDENT(MallocFailure, "malloc_failure")
/* inject fault to simulate transaction abort failure  */
FI_IDENT(AbortTransactionFail, "transaction_abort_failure")
/* inject fault to simulate workfile creation failure  */
FI_IDENT(WorkfileCreationFail, "workfile_creation_failure")
/* inject fault to simulate workfile write failure  */
FI_IDENT(WorkfileWriteFail, "workfile_write_failure")
/* pretend that a query processed billions of rows  */
FI_IDENT(WorkfileHashJoinFailure, "workfile_hashjoin_failure")
/* inject fault before we close workfile in ExecHashJoinNewBatch */
FI_IDENT(ExecutorRunHighProcessed, "executor_run_high_processed")
/* large palloc inside MultiExecHash to attempt to exceed vmem limit */
FI_IDENT(MultiExecHashLargeVmem, "multi_exec_hash_large_vmem")
/* inject fault in ExecSort before doing the actual sort */
FI_IDENT(ExecSortBeforeSorting, "execsort_before_sorting")
/* inject fault in MKSort during the mergeruns phase */
FI_IDENT(ExecSortMKSortMergeRuns, "execsort_mksort_mergeruns")
/* inject fault after shared input scan retrieved a tuple */
FI_IDENT(ExecShareInputNext, "execshare_input_next")
/* inject fault after creation of checkpoint when basebackup requested */
FI_IDENT(BaseBackupPostCreateCheckpoint, "base_backup_post_create_checkpoint")
/* inject fault after compaction, but before the drop of the
 * segment file */
FI_IDENT(CompactionBeforeSegmentFileDropPhase, "compaction_before_segmentfile_drop")
/* inject fault after compaction and drop, but before
 * the cleanup phase for a relation */
FI_IDENT(CompactionBeforeCleanupPhase, "compaction_before_cleanup_phase")
/* inject fault before an append-only insert */
FI_IDENT(AppendOnlyInsert, "appendonly_insert")
/* inject fault before an append-only delete */
FI_IDENT(AppendOnlyDelete, "appendonly_delete")
/* inject fault before an append-only update */
FI_IDENT(AppendOnlyUpdate, "appendonly_update")
/* inject fault while reindex db is in progress */
FI_IDENT(ReindexDB, "reindex_db")
/* inject fault while reindex relation is in progress */
FI_IDENT(ReindexRelation, "reindex_relation")
/* inject fault during scanning of a partition */
FI_IDENT(FaultDuringExecDynamicTableScan, "fault_during_exec_dynamic_table_scan")
/* inject fault at the beginning of rxThreadFunc */
FI_IDENT(FaultInBackgroundWriterMain, "fault_in_background_writer_main")
/* inject fault in cdbCopyStart after dispatch */
FI_IDENT(CdbCopyStartAfterDispatch, "cdb_copy_start_after_dispatch")
/* inject fault at the end of repair_frag */
FI_IDENT(RepairFragEnd, "repair_frag_end")
/* inject fault before truncate in vacuum full */
FI_IDENT(VacuumFullBeforeTruncate, "vacuum_full_before_truncate")
/* inject fault after truncate in vacuum full */
FI_IDENT(VacuumFullAfterTruncate, "vacuum_full_after_truncate")
/* inject fault at the end of first round of vacuumRelation loop */
FI_IDENT(VacuumRelationEndOfFirstRound, "vacuum_relation_end_of_first_round")
/* inject fault during the open relation of the drop phase of vacuumRelation loop */
FI_IDENT(VacuumRelationOpenRelationDuringDropPhase, "vacuum_relation_open_relation_during_drop_phase")
/* inject fault while adding PGPROC to procarray */
FI_IDENT(ProcArray_Add, "procarray_add")
/* inject fault before switching to a new batch in Hash Join */
FI_IDENT(FaultExecHashJoinNewBatch, "exec_hashjoin_new_batch")
/* pause FTS process before committing changes, until shutdown */
FI_IDENT(FtsWaitForShutdown, "fts_wait_for_shutdown")
/* inject fault before cleaning up a runaway query */
FI_IDENT(RunawayCleanup, "runaway_cleanup")
/* inject fault while translating relcache entries */
FI_IDENT(OptRelcacheTranslatorCatalogAccess, "opt_relcache_translator_catalog_access")
/* inject fault before sending QE details during backend initialization */
FI_IDENT(SendQEDetailsInitBackend, "send_qe_details_init_backend")
/* inject fault in ProcessStartupPacket() */
FI_IDENT(ProcessStartupPacketFault, "process_startup_packet")
/* inject fault in quickdie*/
FI_IDENT(QuickDie, "quickdie")
/* inject fault in cdbdisp_dispatchX*/
FI_IDENT(AfterOneSliceDispatched, "after_one_slice_dispatched")
/* inject fault in interconnect to skip sending the stop ack */
FI_IDENT(InterconnectStopAckIsLost, "interconnect_stop_ack_is_lost")
/* inject fault after qe got snapshot and interconnect*/
FI_IDENT(QEGotSnapshotAndInterconnect, "qe_got_snapshot_and_interconnect")
/* inject fault to 'skip' in order to flush all buffers in BgBufferSync() */
FI_IDENT(FsyncCounter, "fsync_counter")
/* inject fault to count buffers fsync'ed by checkpoint process */
FI_IDENT(BgBufferSyncDefaultLogic, "bg_buffer_sync_default_logic")
/* inject fault in FinishPreparedTransaction() right at start of function */
FI_IDENT(FinishPreparedStartOfFunction, "finish_prepared_start_of_function")
/* inject fault in FinishPreparedTransaction() after recording the commit prepared record */
FI_IDENT(FinishPreparedAfterRecordCommitPrepared, "finish_prepared_after_record_commit_prepared")
/* inject fault to report ERROR just after creating Gang */
FI_IDENT(GangCreated, "gang_created")
/* inject fault to report ERROR just after resource group is assigned on master */
FI_IDENT(ResGroupAssignedOnMaster, "resgroup_assigned_on_master")
/* inject fault before reading command */
FI_IDENT(BeforeReadCommand, "before_read_command")
#endif

/*
 * Fault types. These indicate the action to do when the fault injection
 * point is reached.
 */
#ifdef FI_TYPE
FI_TYPE(FaultInjectorTypeNotSpecified = 0, "")
FI_TYPE(FaultInjectorTypeSleep, "sleep")
FI_TYPE(FaultInjectorTypeFault, "fault")
FI_TYPE(FaultInjectorTypeFatal, "fatal")
FI_TYPE(FaultInjectorTypePanic, "panic")
FI_TYPE(FaultInjectorTypeError, "error")
FI_TYPE(FaultInjectorTypeInfiniteLoop, "infinite_loop")
FI_TYPE(FaultInjectorTypeDataCorruption, "data_corruption")
FI_TYPE(FaultInjectorTypeSuspend, "suspend")
FI_TYPE(FaultInjectorTypeResume, "resume")
FI_TYPE(FaultInjectorTypeSkip, "skip")
FI_TYPE(FaultInjectorTypeMemoryFull, "memory_full")
FI_TYPE(FaultInjectorTypeReset, "reset")
FI_TYPE(FaultInjectorTypeStatus, "status")
FI_TYPE(FaultInjectorTypeSegv, "segv")
FI_TYPE(FaultInjectorTypeInterrupt, "interrupt")
FI_TYPE(FaultInjectorTypeFinishPending, "finish_pending")
FI_TYPE(FaultInjectorTypeCheckpointAndPanic, "checkpoint_and_panic")
#endif

/*
 *
 */
#ifdef FI_DDL_STATEMENT
FI_DDL_STATEMENT(DDLNotSpecified = 0, "")
FI_DDL_STATEMENT(CreateDatabase, "create_database")
FI_DDL_STATEMENT(DropDatabase, "drop_database")
FI_DDL_STATEMENT(CreateTable, "create_table")
FI_DDL_STATEMENT(DropTable, "drop_table")
FI_DDL_STATEMENT(CreateIndex, "create_index")
FI_DDL_STATEMENT(AlterIndex, "alter_index")
FI_DDL_STATEMENT(ReIndex, "reindex")
FI_DDL_STATEMENT(DropIndex, "drop_index")
FI_DDL_STATEMENT(CreateTablespaces, "create_tablespaces")
FI_DDL_STATEMENT(DropTablespaces, "drop_tablespaces")
FI_DDL_STATEMENT(Truncate, "truncate")
FI_DDL_STATEMENT(Vacuum, "vacuum")
#endif

/*
 *
 */
#ifdef FI_STATE
FI_STATE(FaultInjectorStateNotInitialized = 0, "not initialized")
/* Request is waiting to be injected */
FI_STATE(FaultInjectorStateWaiting, "set")
/* Fault is injected */
FI_STATE(FaultInjectorStateTriggered, "triggered")
/* Fault was injected and completed successfully */
FI_STATE(FaultInjectorStateCompleted, "completed")
/* Fault was NOT injected */
FI_STATE(FaultInjectorStateFailed, "failed")
#endif
