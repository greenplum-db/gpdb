/*
 *  faultinjector.h
 *  
 *
 *  Copyright 2009-2010, Greenplum Inc. All rights reserved.
 *
 */

#ifndef FAULTINJECTOR_H
#define FAULTINJECTOR_H

#include "pg_config_manual.h"

#define FAULTINJECTOR_MAX_SLOTS	16

#define FAULT_NAME_MAX_LENGTH	256

/*
 *
 */
typedef enum FaultInjectorIdentifier_e {
	FaultInjectorIdNotSpecified=0,
	
	FaultInjectorIdAll,
		/* 
		 * affects all faults injected
		 *		*) remove all faults injected from the segment
		 *		*) display status for all faults injected 
		 */
	Postmaster,
	
	PgControl,
	
	PgXlog,
	
	StartPrepareTx,

	FaultBeforePendingDeleteRelationEntry,

	FaultBeforePendingDeleteDatabaseEntry,

	FaultBeforePendingDeleteTablespaceEntry,

	FaultBeforePendingDeleteFilespaceEntry,
	
	FileRepConsumer,
	
	FileRepConsumerVerification,

	FileRepChangeTrackingCompacting,
	
	FileRepSender,
	
	FileRepReceiver,
	
	FileRepFlush,
	
	FileRepResync,
	
	FileRepResyncInProgress,	
	
	FileRepResyncWorker,
	
	FileRepResyncWorkerRead,
	
	FileRepTransitionToInResyncMirrorReCreate,
	
	FileRepTransitionToInResyncMarkReCreated,
	
	FileRepTransitionToInResyncMarkCompleted,
	
	FileRepTransitionToInSyncBegin,
	
	FileRepTransitionToInSync,
	
	FileRepTransitionToInSyncBeforeCheckpoint,
	
	FileRepTransitionToInSyncMarkCompleted,
	
	FileRepTransitionToChangeTracking,
	FileRepIsOperationCompleted,
	FileRepImmediateShutdownRequested,
	
	Checkpoint,
	
	ChangeTrackingCompactingReport,
	
	ChangeTrackingDisable,
	
	TransactionStartUnderEntryDbSingleton,

	TransactionAbortAfterDistributedPrepared,
	
	TransactionCommitPass1FromCreatePendingToCreated,
	
	TransactionCommitPass1FromDropInMemoryToDropPending,
	
	TransactionCommitPass1FromAbortingCreateNeededToAbortingCreate,
	
	TransactionAbortPass1FromCreatePendingToAbortingCreate,
	
	TransactionAbortPass1FromAbortingCreateNeededToAbortingCreate,
	
	TransactionCommitPass2FromDropInMemoryToDropPending,
	
	TransactionCommitPass2FromAbortingCreateNeededToAbortingCreate,
	
	TransactionAbortPass2FromCreatePendingToAbortingCreate,
	
	TransactionAbortPass2FromAbortingCreateNeededToAbortingCreate,
	
	
	FinishPreparedTransactionCommitPass1FromCreatePendingToCreated,
	
	FinishPreparedTransactionCommitPass2FromCreatePendingToCreated,
		/* commit: create pending => created */
			
	FinishPreparedTransactionAbortPass1FromCreatePendingToAbortingCreate,
	FinishPreparedTransactionAbortPass2FromCreatePendingToAbortingCreate,
		/* abort: create pending => aborting create */

	FinishPreparedTransactionCommitPass1FromDropInMemoryToDropPending,
	
	FinishPreparedTransactionCommitPass2FromDropInMemoryToDropPending,
		/* commit: drop in memory => drop pending */

	FinishPreparedTransactionCommitPass1AbortingCreateNeeded,
	
	FinishPreparedTransactionCommitPass2AbortingCreateNeeded,
		/* commit: create pending => created */
	
	FinishPreparedTransactionAbortPass1AbortingCreateNeeded,
	
	FinishPreparedTransactionAbortPass2AbortingCreateNeeded,	
		/* abort: create pending => aborting create */

	FileRepVerification,
		/* trigger filerep verification for testing */
	
	TwoPhaseTransactionCommitPrepared,
	
	TwoPhaseTransactionAbortPrepared,
	
	DtmBroadcastPrepare,
	
	DtmBroadcastCommitPrepared,
	
	DtmBroadcastAbortPrepared,
	
	DtmXLogDistributedCommit,

	DtmInit,
	
        EndPreparedTwoPhaseSleep,
	       /* sleep after creating two phase files */

	SegmentTransitionRequest,

	SegmentProbeResponse,

	LocalTmRecordTransactionCommit,
	
	MallocFailure,
	AbortTransactionFail,
	WorkfileCreationFail,
	WorkfileWriteFail,
	WorkfileHashJoinFailure,

	ExecutorRunHighProcessed,

	UpdateCommittedEofInPersistentTable,

	MultiExecHashLargeVmem,

	ExecSortBeforeSorting,
	ExecSortMKSortMergeRuns,
	ExecShareInputNext,
	BaseBackupPostCreateCheckpoint,

	CompactionBeforeSegmentFileDropPhase,
	CompactionBeforeCleanupPhase,

	AppendOnlyInsert,
	AppendOnlyDelete,
	AppendOnlyUpdate,

	ReindexDB,
	ReindexRelation,

	FaultDuringExecDynamicTableScan,

	FaultInBackgroundWriterMain,

	CdbCopyStartAfterDispatch,

	RepairFragEnd,
	VacuumFullBeforeTruncate,
	VacuumFullAfterTruncate,
	VacuumRelationEndOfFirstRound,
	VacuumRelationOpenRelationDuringDropPhase,

	RebuildPTDB,

	ProcArray_Add,

	FaultExecHashJoinNewBatch,

	FtsWaitForShutdown,

	RunawayCleanup,

	OptRelcacheTranslatorCatalogAccess,

	SendQEDetailsInitBackend,
	ProcessStartupPacketFault,
	QuickDie,
	AfterOneSliceDispatched,

	InterconnectStopAckIsLost,
	QEGotSnapshotAndInterconnect,

	FsyncCounter,
	BgBufferSyncDefaultLogic,

	FinishPreparedAfterRecordCommitPrepared,

	GangCreated,

	ResGroupAssignedOnMaster,

	BeforeReadCommand,

	/* INSERT has to be done before that line */
	FaultInjectorIdMax,
	
} FaultInjectorIdentifier_e;

/*
 * NB: This list needs to be kept in sync with:
 * - FaultInjectorTypeEnumToString
 * - the help message in clsInjectFault.py
 */
typedef enum FaultInjectorType_e {
	FaultInjectorTypeNotSpecified=0,
	
	FaultInjectorTypeSleep,
	
	FaultInjectorTypeFault,
	
	FaultInjectorTypeFatal,
	
	FaultInjectorTypePanic,
	
	FaultInjectorTypeError,
	
	FaultInjectorTypeInfiniteLoop,
	
	FaultInjectorTypeDataCorruption,
	
	FaultInjectorTypeSuspend,
	
	FaultInjectorTypeResume,
	
	FaultInjectorTypeSkip,
	
	FaultInjectorTypeMemoryFull,
	
	FaultInjectorTypeReset,
	
	FaultInjectorTypeStatus,
	
	FaultInjectorTypeSegv,
	
	FaultInjectorTypeInterrupt,

	FaultInjectorTypeFinishPending,

	FaultInjectorTypeCheckpointAndPanic,

	/* INSERT has to be done before that line */
	FaultInjectorTypeMax,
	
} FaultInjectorType_e;

/*
 *
 */
typedef enum DDLStatement_e {
	DDLNotSpecified=0,
	
	CreateDatabase,
	DropDatabase,
	
	CreateTable,
	DropTable,
	
	CreateIndex,
	AlterIndex,
	ReIndex,
	DropIndex,
	
	CreateFilespaces,
	DropFilespaces,
	
	CreateTablespaces,
	DropTablespaces,
	
	Truncate,
	
	Vacuum,
	
	/* INSERT has to be done before that line */
	DDLMax,
} DDLStatement_e;

/*
 *
 */
typedef enum FaultInjectorState_e {
	FaultInjectorStateNotInitialized=0,
		
	FaultInjectorStateWaiting,
		/* Request is waiting to be injected */
	
	FaultInjectorStateTriggered,
		/* Fault is injected */
	
	FaultInjectorStateCompleted,
		/* Fault was injected and completed successfully */
	
	FaultInjectorStateFailed,
		/* Fault was NOT injected */
	
} FaultInjectorState_e;


/*
 *
 */
typedef struct FaultInjectorEntry_s {
	
	char						faultName[FAULT_NAME_MAX_LENGTH];

	FaultInjectorIdentifier_e	faultInjectorIdentifier;
	
	FaultInjectorType_e		faultInjectorType;
	
	int						sleepTime;
		/* in seconds, in use if fault injection type is sleep */
		
	DDLStatement_e			ddlStatement;
	
	char					databaseName[NAMEDATALEN];
	
	char					tableName[NAMEDATALEN];
	
	volatile	int			occurrence;
	volatile	 int	numTimesTriggered;
	volatile	FaultInjectorState_e	faultInjectorState;

		/* the state of the fault injection */
	char					bufOutput[2500];
	
} FaultInjectorEntry_s;


extern FaultInjectorType_e	FaultInjectorTypeStringToEnum(
									char*		faultTypeString);

extern	FaultInjectorIdentifier_e FaultInjectorIdentifierStringToEnum(
									char*			faultName);

extern DDLStatement_e FaultInjectorDDLStringToEnum(
									char*	ddlString);

extern Size FaultInjector_ShmemSize(void);

extern void FaultInjector_ShmemInit(void);

extern FaultInjectorType_e FaultInjector_InjectFaultNameIfSet(
							   const char*				 faultName,
							   DDLStatement_e			 ddlStatement,
							   const char*				 databaseName,
							   const char*				 tableName);

extern FaultInjectorType_e FaultInjector_InjectFaultIfSet(
							   FaultInjectorIdentifier_e identifier,
							   DDLStatement_e			 ddlStatement,
							   const char*				 databaseName,
							   const char*				 tableName);

extern int FaultInjector_SetFaultInjection(
							FaultInjectorEntry_s	*entry);


extern bool FaultInjector_IsFaultInjected(
							char* faultName);


#ifdef FAULT_INJECTOR
#define SIMPLE_FAULT_INJECTOR(FaultIdentifier) \
	FaultInjector_InjectFaultIfSet(FaultIdentifier, DDLNotSpecified, "", "")
#define SIMPLE_FAULT_NAME_INJECTOR(FaultName) \
	FaultInjector_InjectFaultNameIfSet(FaultName, DDLNotSpecified, "", "")
#else
#define SIMPLE_FAULT_INJECTOR(FaultIdentifier)
#define SIMPLE_FAULT_NAME_INJECTOR(FaultName)
#endif

#endif	/* FAULTINJECTOR_H */

