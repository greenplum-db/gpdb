/*-------------------------------------------------------------------------
 *
 * cdbtm.c
 *	  Provides routines for performing distributed transaction
 *
 * Portions Copyright (c) 2005-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbtm.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include "catalog/pg_authid.h"
#include "cdb/cdbtm.h"
#include "libpq/libpq-be.h"
#include "miscadmin.h"
#include "storage/shmem.h"
#include "storage/ipc.h"
#include "cdb/cdbdisp.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdisp_dtx.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbdtxcontextinfo.h"

#include "cdb/cdbvars.h"
#include "access/transam.h"
#include "access/xact.h"
#include "libpq-fe.h"
#include "libpq-int.h"
#include "cdb/cdbfts.h"
#include "lib/stringinfo.h"
#include "access/twophase.h"
#include "access/distributedlog.h"
#include "postmaster/postmaster.h"
#include "storage/procarray.h"
#include "cdb/cdbpersistentrecovery.h"
#include "cdb/cdbpersistentcheck.h"

#include "cdb/cdbllize.h"
#include "utils/faultinjector.h"
#include "utils/fmgroids.h"
#include "utils/sharedsnapshot.h"
#include "utils/snapmgr.h"

extern bool Test_print_direct_dispatch_info;
extern struct Port *MyProcPort;

#define DTM_DEBUG3 (Debug_print_full_dtm ? LOG : DEBUG3)
#define DTM_DEBUG5 (Debug_print_full_dtm ? LOG : DEBUG5)

/*
 * Directory where Utility Mode DTM REDO file reside within PGDATA
 */
#define UTILITYMODEDTMREDO_DIR "pg_utilitymodedtmredo"

/*
 * File name for Utility Mode DTM REDO
 */
#define UTILITYMODEDTMREDO_FILE "savedtmredo.file"

static LWLockId shmControlLock;
static volatile bool *shmTmRecoverred;
static volatile DistributedTransactionTimeStamp *shmDistribTimeStamp;
static volatile DistributedTransactionId *shmGIDSeq = NULL;
static volatile int *shmNumGxacts;

static int	ControlLockCount = 0;

uint32	   *shmNextSnapshotId;

volatile bool *shmDtmStarted;

/* global transaction array */
static TMGXACT **shmGxactArray;

/**
 * This pointer into shared memory is on the QD, and represents the current open transaction.
 */
static TMGXACT *currentGxact;

static int	max_tm_gxacts = 100;

static int	redoFileFD = -1;
static int	redoFileOffset;

typedef struct InDoubtDtx
{
	char		gid[TMGIDSIZE];
} InDoubtDtx;


/* here are some flag options relationed to the txnOptions field of
 * PQsendGpQuery
 */

/* bit 1 is for statement wants DTX transaction
 *
 * bits 2-3 for iso level  00 read-committed
 *						   01 read-uncommitted
 *						   10 repeatable-read
 *						   11 serializable
 * bit 4 is for read-only
 */
#define GP_OPT_NEED_TWO_PHASE                           0x0001

#define GP_OPT_READ_COMMITTED    						0x0002
#define GP_OPT_READ_UNCOMMITTED  						0x0004
#define GP_OPT_REPEATABLE_READ   						0x0006
#define GP_OPT_SERIALIZABLE 	  						0x0008

#define GP_OPT_READ_ONLY         						0x0010

#define GP_OPT_EXPLICT_BEGIN      						0x0020

/*=========================================================================
 * FUNCTIONS PROTOTYPES
 */
static void initGxact(TMGXACT *gxact);
static void releaseGxact_UnderLocks(void);
static void releaseGxact(void);
static void generateGID(char *gid, DistributedTransactionId *gxid);

static void recoverTM(void);
static bool recoverInDoubtTransactions(void);
static HTAB *gatherRMInDoubtTransactions(void);
static void abortRMInDoubtTransactions(HTAB *htab);

static void dumpAllDtx(void);

/* static void resolveInDoubtDtx(void); */
static void dumpRMOnlyDtx(HTAB *htab, StringInfoData *buff);

static bool doDispatchDtxProtocolCommand(DtxProtocolCommand dtxProtocolCommand, int flags,
							 char *gid, DistributedTransactionId gxid,
							 bool *badGangs, bool raiseError, CdbDispatchDirectDesc *direct,
							 char *serializedDtxContextInfo, int serializedDtxContextInfoLen);
static void doPrepareTransaction(void);
static void doInsertForgetCommitted(void);
static void doNotifyingCommitPrepared(void);
static void doNotifyingAbort(void);
static void retryAbortPrepared(void);
static bool doNotifyCommittedInDoubt(char *gid);
static void doAbortInDoubt(char *gid);
static void doQEDistributedExplicitBegin(int txnOptions);

static bool isDtxQueryDispatcher(void);
static void UtilityModeSaveRedo(bool committed, TMGXACT_LOG *gxact_log);
static void ReplayRedoFromUtilityMode(void);
static void RemoveRedoUtilityModeFile(void);
static void performDtxProtocolCommitPrepared(const char *gid, bool raiseErrorIfNotFound);
static void performDtxProtocolAbortPrepared(const char *gid, bool raiseErrorIfNotFound);

extern void resetSessionForPrimaryGangLoss(bool resetSession);
extern void CheckForResetSession(void);

/**
 * All assignments of the global DistributedTransactionContext should go through this function
 *   (so we can add logging here to see all assignments)
 *
 * @param context the new value for DistributedTransactionContext
 */
static void
setDistributedTransactionContext(DtxContext context)
{
	/*
	 * elog(INFO, "Setting DistributedTransactionContext to '%s'",
	 * DtxContextToString(context));
	 */
	DistributedTransactionContext = context;
}

static void
requireDistributedTransactionContext(DtxContext requiredCurrentContext)
{
	if (DistributedTransactionContext != requiredCurrentContext)
	{
		elog(FATAL, "Expected segment distributed transaction context to be '%s', found '%s'",
			 DtxContextToString(requiredCurrentContext),
			 DtxContextToString(DistributedTransactionContext));
	}
}

static void
setGxactState(TMGXACT *transaction, DtxState state)
{
	Assert(transaction != NULL);

	/*
	 * elog(INFO, "Setting transaction state to '%s'",
	 * DtxStateToString(state));
	 */
	transaction->state = state;
}

/**
 * All assignments of currentGxact->state should go through this function
 *   (so we can add logging here to see all assignments)
 *
 * This should only be called when currentGxact is non-NULL
 *
 * @param state the new value for currentGxact->state
 */
static void
setCurrentGxactState(DtxState state)
{
	setGxactState(currentGxact, state);
}

/**
 * Does DistributedTransactionContext indicate that this is acting as a QD?
 */
static bool
isQDContext(void)
{
	switch (DistributedTransactionContext)
	{
		case DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE:
		case DTX_CONTEXT_QD_RETRY_PHASE_2:
			return true;
		default:
			return false;
	}
}

/**
 * Does DistributedTransactionContext indicate that this is acting as a QE?
 */
static bool
isQEContext()
{
	switch (DistributedTransactionContext)
	{
		case DTX_CONTEXT_QE_ENTRY_DB_SINGLETON:
		case DTX_CONTEXT_QE_AUTO_COMMIT_IMPLICIT:
		case DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER:
		case DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER:
		case DTX_CONTEXT_QE_READER:
			return true;
		default:
			return false;
	}
}

/*=========================================================================
 * VISIBLE FUNCTIONS
 */

DistributedTransactionTimeStamp
getDtxStartTime(void)
{
	if (shmDistribTimeStamp != NULL)
		return *shmDistribTimeStamp;
	else
		return 0;
}

DistributedTransactionId
getDistributedTransactionId(void)
{
	if (isQDContext())
	{
		return currentGxact == NULL
			? InvalidDistributedTransactionId
			: currentGxact->gxid;
	}
	else if (isQEContext())
	{
		return QEDtxContextInfo.distributedXid;
	}
	else
	{
		return InvalidDistributedTransactionId;
	}
}

bool
getDistributedTransactionIdentifier(char *id)
{
	if (isQDContext())
	{
		if (currentGxact != NULL)
		{
			/*
			 * The length check here requires the identifer have a trailing
			 * NUL character.
			 */
			if (strlen(currentGxact->gid) >= TMGIDSIZE)
				elog(PANIC, "Distribute transaction identifier too long (%d)",
					 (int) strlen(currentGxact->gid));
			memcpy(id, currentGxact->gid, TMGIDSIZE);
			return true;
		}
	}
	else if (isQEContext())
	{
		if (QEDtxContextInfo.distributedXid != InvalidDistributedTransactionId)
		{
			if (strlen(QEDtxContextInfo.distributedId) >= TMGIDSIZE)
				elog(PANIC, "Distribute transaction identifier too long (%d)",
					 (int) strlen(QEDtxContextInfo.distributedId));
			memcpy(id, QEDtxContextInfo.distributedId, TMGIDSIZE);
			return true;
		}
	}

	MemSet(id, 0, TMGIDSIZE);
	return false;
}

bool
isPreparedDtxTransaction(void)
{
	if (Gp_role != GP_ROLE_DISPATCH ||
		DistributedTransactionContext != DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE ||
		currentGxact == NULL)
		return false;

	return (currentGxact->state == DTX_STATE_PREPARED);
}

void
getDtxLogInfo(TMGXACT_LOG *gxact_log)
{
	if (currentGxact == NULL)
	{
		elog(FATAL, "getDtxLogInfo found current distributed transaction is NULL");
	}

	if (strlen(currentGxact->gid) >= TMGIDSIZE)
		elog(PANIC, "Distribute transaction identifier too long (%d)",
			 (int) strlen(currentGxact->gid));
	memcpy(gxact_log->gid, currentGxact->gid, TMGIDSIZE);
	gxact_log->gxid = currentGxact->gxid;
}

bool
notifyCommittedDtxTransactionIsNeeded(void)
{
	if (DistributedTransactionContext != DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE)
	{
		elog(DTM_DEBUG5, "notifyCommittedDtxTransaction nothing to do (DistributedTransactionContext = '%s')",
			 DtxContextToString(DistributedTransactionContext));
		return false;
	}

	if (currentGxact == NULL)
	{
		elog(DTM_DEBUG5, "notifyCommittedDtxTransaction nothing to do (currentGxact == NULL)");
		return false;
	}

	return true;
}

/*
 * Notify commited a global transaction, called by user commit
 * or by CommitTransaction
 */
void
notifyCommittedDtxTransaction(void)
{
	Assert(DistributedTransactionContext == DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE);

	Assert(currentGxact != NULL);

	doNotifyingCommitPrepared();
}

static inline void
copyDirectDispatchFromTransaction(CdbDispatchDirectDesc *dOut)
{
	if (currentGxact->directTransaction)
	{
		dOut->directed_dispatch = true;
		dOut->count = 1;
		dOut->content[0] = currentGxact->directTransactionContentId;
	}
	else
	{
		dOut->directed_dispatch = false;
	}
}

static bool
GetRootNodeIsDirectDispatch(PlannedStmt *stmt)
{
	if (stmt == NULL)
		return false;

	if (stmt->planTree == NULL)
		return false;

	return stmt->planTree->directDispatch.isDirectDispatch;
}

/**
 * note that the ability to look at the root node of a plan in order to determine
 *    direct dispatch overall depends on the way we assign direct dispatch.  Parent slices are
 *    never more directed than child slices.  This could be fixed with an iteration over all slices and
 *    combine from every slice.
 *
 * return true IFF the directDispatch data stored in n should be applied to the transaction
 */
static bool
GetPlannedStmtDirectDispatch_AndUsingNodeIsSufficient(PlannedStmt *stmt)
{
	if (!GetRootNodeIsDirectDispatch(stmt))
		return false;

	/*
	 * now look at number initplans .. we do something simple.  ANY initPlans
	 * means we don't do directDispatch at the dtm level.  It's technically
	 * possible that the initPlan and the node share the same direct dispatch
	 * set but we don't bother right now.
	 */
	if (stmt->nInitPlans > 0)
		return false;

	return true;
}

/*
 * @param needsTwoPhaseCommit if true then marks the current Distributed Transaction as needing to use the
 *       2 phase commit protocol.
 */
void
dtmPreCommand(const char *debugCaller, const char *debugDetail, PlannedStmt *stmt,
			  bool needsTwoPhaseCommit, bool wantSnapshot, bool inCursor)
{
	bool		needsPromotionFromDirectDispatch = false;
	const bool	rootNodeIsDirectDispatch = GetRootNodeIsDirectDispatch(stmt);
	const bool	nodeSaysDirectDispatch = GetPlannedStmtDirectDispatch_AndUsingNodeIsSufficient(stmt);

	Assert(debugCaller != NULL);
	Assert(debugDetail != NULL);

	/**
	 * update the information about what segments are participating in the transaction
	 */
	if (currentGxact == NULL)
	{
		/* no open transaction so don't do anything */
	}
	else if (currentGxact->state == DTX_STATE_ACTIVE_NOT_DISTRIBUTED)
	{
		/* Can we direct this transaction to a single content-id ? */
		if (nodeSaysDirectDispatch)
		{
			currentGxact->directTransaction = true;
			currentGxact->directTransactionContentId = linitial_int(stmt->planTree->directDispatch.contentIds);

			elog(DTM_DEBUG5,
				 "dtmPreCommand going distributed (to content %d) for gid = %s (%s, detail = '%s')",
				 currentGxact->directTransactionContentId, currentGxact->gid, debugCaller, debugDetail);
		}
		else
		{
			currentGxact->directTransaction = false;

			if (rootNodeIsDirectDispatch)
			{
				/*
				 * implicit write on the root, but some initPlan was to all
				 * contents...so send explicit start
				 */
				needsPromotionFromDirectDispatch = true;
			}

			elog(DTM_DEBUG5,
				 "dtmPreCommand going distributed (all gangs) for gid = %s (%s, detail = '%s')",
				 currentGxact->gid, debugCaller, debugDetail);
		}
	}
	else if (currentGxact->state == DTX_STATE_ACTIVE_DISTRIBUTED)
	{
		bool		wasDirected = currentGxact->directTransaction;
		int			wasPromotedFromDirectDispatchContentId = wasDirected ? currentGxact->directTransactionContentId : -1;

		/* Can we still direct this transaction to a single content-id ? */
		if (currentGxact->directTransaction)
		{
			currentGxact->directTransaction = false;
			/* turn off, but may be restored below */

			if (nodeSaysDirectDispatch)
			{
				int			contentId = linitial_int(stmt->planTree->directDispatch.contentIds);

				if (contentId == currentGxact->directTransactionContentId)
				{
					/*
					 * it was the same content!  Stay in a single direct
					 * transaction
					 */
					currentGxact->directTransaction = true;
				}
			}
		}

		if (currentGxact->directTransaction)
		{
			/** was not actually promoted */
			wasPromotedFromDirectDispatchContentId = -1;
		}

		if (wasPromotedFromDirectDispatchContentId != -1)
			needsPromotionFromDirectDispatch = true;

		elog(DTM_DEBUG5,
			 "dtmPreCommand gid = %s is already distributed (%s, detail = '%s'), (was %s : now %s)",
			 currentGxact->gid, debugCaller, debugDetail,
			 wasDirected ? "directed" : "all gangs",
			 currentGxact->directTransaction ? "directed" : "all gangs"
			);
	}

	/**
	 * If two-phase commit then begin transaction.
	 */
	if (needsTwoPhaseCommit)
	{
		if (currentGxact == NULL)
		{
			elog(ERROR, "DTM transaction is not active (%s, detail = '%s')", debugCaller, debugDetail);
		}
		else if (currentGxact->state == DTX_STATE_ACTIVE_NOT_DISTRIBUTED)
		{
			setCurrentGxactState(DTX_STATE_ACTIVE_DISTRIBUTED);
		}
		else if (currentGxact->state == DTX_STATE_ACTIVE_DISTRIBUTED)
		{
			/* already distributed, no need to change */
		}
		else
		{
			elog(ERROR, "DTM transaction is not active (state = %s, %s, detail = '%s')",
				 DtxStateToString(currentGxact->state), debugCaller, debugDetail);
		}
	}

	/**
	 * If promotion from direct-dispatch to whole-cluster dispatch was done then tell about it.
	 *
	 * FUTURE: note that this is only needed if the query we are going to run would not itself
	 *   do this (that is, if the query we are going to run is a read-only one)
	 */
	if (currentGxact &&
		currentGxact->state == DTX_STATE_ACTIVE_DISTRIBUTED &&
		needsPromotionFromDirectDispatch)
	{
		CdbDispatchDirectDesc direct = default_dispatch_direct_desc;
		char	   *serializedDtxContextInfo;
		int			serializedDtxContextInfoLen;
		bool		badGangs,
					succeeded;

		serializedDtxContextInfo = qdSerializeDtxContextInfo(&serializedDtxContextInfoLen, wantSnapshot, inCursor,
															 mppTxnOptions(true), "promoteTransactionIn_dtmPreCommand");

		succeeded = doDispatchDtxProtocolCommand(DTX_PROTOCOL_COMMAND_STAY_AT_OR_BECOME_IMPLIED_WRITER, /* flags */ 0,
												 currentGxact->gid, currentGxact->gxid,
												 &badGangs, /* raiseError */ false, &direct,
												 serializedDtxContextInfo, serializedDtxContextInfoLen);

		/* send a DTM command to others to tell them about the transaction */
		if (!succeeded)
		{
			ereport(ERROR, (errmsg("Global transaction upgrade from single segment to entire cluster failed for gid = \"%s\" due to error",
								   currentGxact->gid)));
		}
	}
}


/*
 * Routine to dispatch internal sub-transaction calls from UDFs to segments.
 * The calls are BeginInternalSubTransaction, ReleaseCurrentSubTransaction and
 * RollbackAndReleaseCurrentSubTransaction.
 */
bool
doDispatchSubtransactionInternalCmd(DtxProtocolCommand cmdType)
{
	CdbDispatchDirectDesc direct = default_dispatch_direct_desc;
	char	   *serializedDtxContextInfo = NULL;
	int			serializedDtxContextInfoLen = 0;
	bool		badGangs,
				succeeded = false;

	if (cmdType == DTX_PROTOCOL_COMMAND_SUBTRANSACTION_BEGIN_INTERNAL)
	{
		getTmLock();
		if (currentGxact->state == DTX_STATE_ACTIVE_NOT_DISTRIBUTED)
		{
			setCurrentGxactState(DTX_STATE_ACTIVE_DISTRIBUTED);
		}
		releaseTmLock();
	}

	serializedDtxContextInfo = qdSerializeDtxContextInfo(
														 &serializedDtxContextInfoLen,
														 false /* wantSnapshot */ ,
														 false /* inCursor */ ,
														 mppTxnOptions(true),
														 "doDispatchSubtransactionInternalCmd");

	succeeded = doDispatchDtxProtocolCommand(
											 cmdType, /* flags */ 0,
											 currentGxact->gid, currentGxact->gxid,
											 &badGangs, /* raiseError */ true, &direct,
											 serializedDtxContextInfo, serializedDtxContextInfoLen);

	/* send a DTM command to others to tell them about the transaction */
	if (!succeeded)
	{
		ereport(ERROR,
				(errmsg(
						"dispatching subtransaction internal command failed for gid = \"%s\" due to error",
						currentGxact->gid)));
	}

	return succeeded;
}

/*
 * The executor can avoid starting a distributed transaction if it knows that
 * the current dtx is clean and we aren't in a user-started global transaction.
 */
bool
isCurrentDtxTwoPhase(void)
{
	if (currentGxact == NULL)
	{
		return false;
	}
	else
	{
		return currentGxact->state == DTX_STATE_ACTIVE_DISTRIBUTED;
	}
}

DtxState
getCurrentDtxState(void)
{
	if (currentGxact != NULL)
		return currentGxact->state;

	return DTX_STATE_NONE;
}

static void
doPrepareTransaction(void)
{
	bool		succeeded;
	CdbDispatchDirectDesc direct = default_dispatch_direct_desc;

	CHECK_FOR_INTERRUPTS();

	elog(DTM_DEBUG5, "doPrepareTransaction entering in state = %s",
		 DtxStateToString(currentGxact->state));

	/*
	 * Don't allow a cancel while we're dispatching our prepare (we wrap our
	 * state change as well; for good measure.
	 */
	HOLD_INTERRUPTS();

	copyDirectDispatchFromTransaction(&direct);

	getTmLock();
	Assert(currentGxact->state == DTX_STATE_ACTIVE_DISTRIBUTED);
	setCurrentGxactState(DTX_STATE_PREPARING);
	releaseTmLock();

	elog(DTM_DEBUG5, "doPrepareTransaction moved to state = %s", DtxStateToString(currentGxact->state));

	succeeded = doDispatchDtxProtocolCommand(DTX_PROTOCOL_COMMAND_PREPARE, /* flags */ 0,
											 currentGxact->gid, currentGxact->gxid,
											 &currentGxact->badPrepareGangs, /* raiseError */ false, &direct, NULL, 0);

	/*
	 * Now we've cleaned up our dispatched statement, cancels are allowed
	 * again.
	 */
	RESUME_INTERRUPTS();

	if (!succeeded)
	{
		elog(DTM_DEBUG5, "doPrepareTransaction error finds badPrimaryGangs = %s",
			 (currentGxact->badPrepareGangs ? "true" : "false"));
		elog(ERROR, "The distributed transaction 'Prepare' broadcast failed to one or more segments for gid = %s.",
			 currentGxact->gid);
	}
	elog(DTM_DEBUG5, "The distributed transaction 'Prepare' broadcast succeeded to the segments for gid = %s.",
		 currentGxact->gid);

	getTmLock();
	Assert(currentGxact->state == DTX_STATE_PREPARING);
	setCurrentGxactState(DTX_STATE_PREPARED);
	releaseTmLock();

	SIMPLE_FAULT_INJECTOR(DtmBroadcastPrepare);

	elog(DTM_DEBUG5, "doPrepareTransaction leaving in state = %s", DtxStateToString(currentGxact->state));
}

/*
 * Insert FORGET COMMITTED into the xlog.
 * Call with both ProcArrayLock and DTM lock already held.
 */
static void
doInsertForgetCommitted(void)
{
	TMGXACT_LOG gxact_log;

	elog(DTM_DEBUG5, "doInsertForgetCommitted entering in state = %s", DtxStateToString(currentGxact->state));

	setCurrentGxactState(DTX_STATE_INSERTING_FORGET_COMMITTED);

	if (strlen(currentGxact->gid) >= TMGIDSIZE)
		elog(PANIC, "Distribute transaction identifier too long (%d)",
			 (int) strlen(currentGxact->gid));
	memcpy(&gxact_log.gid, currentGxact->gid, TMGIDSIZE);
	gxact_log.gxid = currentGxact->gxid;

	RecordDistributedForgetCommitted(&gxact_log);

	setCurrentGxactState(DTX_STATE_INSERTED_FORGET_COMMITTED);

	/*
	 * These two actions must be performed for a distributed transaction under
	 * the same locking of ProceArrayLock so the visibility of the transaction
	 * changes for local master readers (e.g. those using  SnapshotNow for
	 * reading) the same as for distributed transactions.
	 */
	ClearTransactionFromPgProc_UnderLock(MyProc, true);
	releaseGxact_UnderLocks();

	elog(DTM_DEBUG5, "doInsertForgetCommitted called releaseGxact");
}

static void
doNotifyingCommitPrepared(void)
{
	bool		succeeded;
	bool		badGangs;
	int			retry = 0;
	volatile int savedInterruptHoldoffCount;

	CdbDispatchDirectDesc direct = default_dispatch_direct_desc;

	elog(DTM_DEBUG5, "doNotifyingCommitPrepared entering in state = %s", DtxStateToString(currentGxact->state));

	getTmLock();

	copyDirectDispatchFromTransaction(&direct);

	Assert(currentGxact->state == DTX_STATE_FORCED_COMMITTED);
	setCurrentGxactState(DTX_STATE_NOTIFYING_COMMIT_PREPARED);
	releaseTmLock();

	if (strlen(currentGxact->gid) >= TMGIDSIZE)
		elog(PANIC, "Distribute transaction identifier too long (%d)",
			 (int) strlen(currentGxact->gid));

	SIMPLE_FAULT_INJECTOR(DtmBroadcastCommitPrepared);
	savedInterruptHoldoffCount = InterruptHoldoffCount;

	PG_TRY();
	{
		succeeded = doDispatchDtxProtocolCommand(DTX_PROTOCOL_COMMAND_COMMIT_PREPARED, /* flags */ 0,
												 currentGxact->gid, currentGxact->gxid,
												 &badGangs, /* raiseError */ false,
												 &direct, NULL, 0);
	}
	PG_CATCH();
	{
		/*
		 * restore the previous value, which is reset to 0 in errfinish.
		 */
		InterruptHoldoffCount = savedInterruptHoldoffCount;
		succeeded = false;
	}
	PG_END_TRY();

	if (!succeeded)
	{
		getTmLock();
		Assert(currentGxact->state == DTX_STATE_NOTIFYING_COMMIT_PREPARED);
		elog(DTM_DEBUG5, "marking retry needed for distributed transaction"
			 " 'Commit Prepared' broadcast to the segments for gid = %s.",
			 currentGxact->gid);
		setCurrentGxactState(DTX_STATE_RETRY_COMMIT_PREPARED);
		setDistributedTransactionContext(DTX_CONTEXT_QD_RETRY_PHASE_2);
		releaseTmLock();
	}

	while (!succeeded && dtx_phase2_retry_count > retry++)
	{
		elog(WARNING, "the distributed transaction 'Commit Prepared' broadcast "
			 "failed to one or more segments for gid = %s.  Retrying ... try %d",
			 currentGxact->gid, retry);

		/*
		 * We must succeed in delivering the commit to all segment instances,
		 * or any failed segment instances must be marked INVALID.
		 */
		elog(NOTICE, "Releasing segworker group to retry broadcast.");
		DisconnectAndDestroyAllGangs(true);

		/*
		 * This call will at a minimum change the session id so we will not
		 * have SharedSnapshotAdd colissions.
		 */
		CheckForResetSession();
		savedInterruptHoldoffCount = InterruptHoldoffCount;

		PG_TRY();
		{
			succeeded = doDispatchDtxProtocolCommand(
													 DTX_PROTOCOL_COMMAND_RETRY_COMMIT_PREPARED, /* flags */ 0,
													 currentGxact->gid, currentGxact->gxid,
													 &badGangs, /* raiseError */ false,
													 &direct, NULL, 0);
		}
		PG_CATCH();
		{
			/*
			 * restore the previous value, which is reset to 0 in errfinish.
			 */
			InterruptHoldoffCount = savedInterruptHoldoffCount;
			succeeded = false;
		}
		PG_END_TRY();
	}

	if (!succeeded)
		elog(PANIC, "unable to complete 'Commit Prepared' broadcast for gid = %s",
			 currentGxact->gid);
	elog(DTM_DEBUG5, "the distributed transaction 'Commit Prepared' broadcast "
		 "succeeded to all the segments for gid = %s.", currentGxact->gid);

	/*
	 * Global locking order: ProcArrayLock then DTM lock since calls
	 * doInsertForgetCommitted calls releaseGxact.
	 */
	LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

	getTmLock();

	doInsertForgetCommitted();

	releaseTmLock();

	LWLockRelease(ProcArrayLock);
}

static void
retryAbortPrepared(void)
{
	int			retry = 0;
	bool		succeeded = false;
	bool		badGangs = false;
	volatile int savedInterruptHoldoffCount;

	CdbDispatchDirectDesc direct = default_dispatch_direct_desc;

	while (!succeeded && dtx_phase2_retry_count > retry++)
	{
		/*
		 * By deallocating the gang, we will force a new gang to connect to
		 * all the segment instances.  And, we will abort the transactions in
		 * the segments. What's left are possibily prepared transactions.
		 */
		elog(NOTICE, "Releasing segworker groups to retry broadcast.");
		DisconnectAndDestroyAllGangs(true);

		/*
		 * This call will at a minimum change the session id so we will not
		 * have SharedSnapshotAdd colissions.
		 */
		CheckForResetSession();

		savedInterruptHoldoffCount = InterruptHoldoffCount;

		PG_TRY();
		{
			succeeded = doDispatchDtxProtocolCommand(
													 DTX_PROTOCOL_COMMAND_RETRY_ABORT_PREPARED, /* flags */ 0,
													 currentGxact->gid, currentGxact->gxid,
													 &badGangs, /* raiseError */ false,
													 &direct, NULL, 0);
			if (!succeeded)
				elog(WARNING, "the distributed transaction 'Abort' broadcast "
					 "failed to one or more segments for gid = %s.  "
					 "Retrying ... try %d", currentGxact->gid, retry);
		}
		PG_CATCH();
		{
			/*
			 * restore the previous value, which is reset to 0 in errfinish.
			 */
			InterruptHoldoffCount = savedInterruptHoldoffCount;
			succeeded = false;
		}
		PG_END_TRY();
	}

	if (!succeeded)
		elog(PANIC, "unable to complete 'Abort' broadcast for gid = %s",
			 currentGxact->gid);
	elog(DTM_DEBUG5, "The distributed transaction 'Abort' broadcast succeeded to "
		 "all the segments for gid = %s.", currentGxact->gid);
}


static void
doNotifyingAbort(void)
{
	bool		succeeded;
	bool		badGangs;
	volatile int savedInterruptHoldoffCount;

	CdbDispatchDirectDesc direct = default_dispatch_direct_desc;

	elog(DTM_DEBUG5, "doNotifyingAborted entering in state = %s", DtxStateToString(currentGxact->state));

	getTmLock();
	Assert(currentGxact->state == DTX_STATE_NOTIFYING_ABORT_NO_PREPARED ||
		   currentGxact->state == DTX_STATE_NOTIFYING_ABORT_SOME_PREPARED ||
		   currentGxact->state == DTX_STATE_NOTIFYING_ABORT_PREPARED);
	releaseTmLock();

	copyDirectDispatchFromTransaction(&direct);

	if (currentGxact->state == DTX_STATE_NOTIFYING_ABORT_NO_PREPARED)
	{
		if (GangsExist())
		{
			succeeded = doDispatchDtxProtocolCommand(DTX_PROTOCOL_COMMAND_ABORT_NO_PREPARED, /* flags */ 0,
													 currentGxact->gid, currentGxact->gxid,
													 &badGangs, /* raiseError */ false,
													 &direct, NULL, 0);
			if (!succeeded)
			{
				elog(WARNING, "The distributed transaction 'Abort' broadcast failed to one or more segments for gid = %s.",
					 currentGxact->gid);

				/*
				 * Reset the dispatch logic and disconnect from any segment
				 * that didn't respond to our abort.
				 */
				elog(NOTICE, "Releasing segworker groups to finish aborting the transaction.");
				DisconnectAndDestroyAllGangs(true);

				/*
				 * This call will at a minimum change the session id so we
				 * will not have SharedSnapshotAdd colissions.
				 */
				CheckForResetSession();
			}
			else
			{
				elog(DTM_DEBUG5,
					 "The distributed transaction 'Abort' broadcast succeeded to all the segments for gid = %s.",
					 currentGxact->gid);
			}
		}
		else
		{
			elog(DTM_DEBUG5,
				 "The distributed transaction 'Abort' broadcast was omitted (segworker group already dead) gid = %s.",
				 currentGxact->gid);
		}
	}
	else
	{
		DtxProtocolCommand dtxProtocolCommand;
		char	   *abortString;
		int			retry = 0;

		Assert(currentGxact->state == DTX_STATE_NOTIFYING_ABORT_SOME_PREPARED ||
			   currentGxact->state == DTX_STATE_NOTIFYING_ABORT_PREPARED);

		if (currentGxact->state == DTX_STATE_NOTIFYING_ABORT_SOME_PREPARED)
		{
			dtxProtocolCommand = DTX_PROTOCOL_COMMAND_ABORT_SOME_PREPARED;
			abortString = "Abort [Prepared]";
		}
		else
		{
			dtxProtocolCommand = DTX_PROTOCOL_COMMAND_ABORT_PREPARED;
			abortString = "Abort Prepared";
		}

		savedInterruptHoldoffCount = InterruptHoldoffCount;

		PG_TRY();
		{
			succeeded = doDispatchDtxProtocolCommand(dtxProtocolCommand, /* flags */ 0,
													 currentGxact->gid, currentGxact->gxid,
													 &badGangs, /* raiseError */ false,
													 &direct, NULL, 0);
		}
		PG_CATCH();
		{
			/*
			 * restore the previous value, which is reset to 0 in errfinish.
			 */
			InterruptHoldoffCount = savedInterruptHoldoffCount;
			succeeded = false;
		}
		PG_END_TRY();

		if (!succeeded)
		{
			elog(WARNING, "the distributed transaction '%s' broadcast failed"
				 " to one or more segments for gid = %s.  Retrying ... try %d",
				 abortString, currentGxact->gid, retry);

			getTmLock();
			setCurrentGxactState(DTX_STATE_RETRY_ABORT_PREPARED);
			setDistributedTransactionContext(DTX_CONTEXT_QD_RETRY_PHASE_2);
			releaseTmLock();
		}
		retryAbortPrepared();
	}

	SIMPLE_FAULT_INJECTOR(DtmBroadcastAbortPrepared);

	/*
	 * Global locking order: ProcArrayLock then DTM lock.
	 */
	LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

	getTmLock();

	Assert(currentGxact->state == DTX_STATE_NOTIFYING_ABORT_NO_PREPARED ||
		   currentGxact->state == DTX_STATE_NOTIFYING_ABORT_SOME_PREPARED ||
		   currentGxact->state == DTX_STATE_NOTIFYING_ABORT_PREPARED ||
		   currentGxact->state == DTX_STATE_RETRY_ABORT_PREPARED);
	releaseGxact_UnderLocks();
	elog(DTM_DEBUG5, "doNotifyingAbort called releaseGxact");

	releaseTmLock();

	LWLockRelease(ProcArrayLock);
}

static bool
doNotifyCommittedInDoubt(char *gid)
{
	bool		succeeded;
	bool		badGangs;

	CdbDispatchDirectDesc direct = default_dispatch_direct_desc;

	/* UNDONE: Pass real gxid instead of InvalidDistributedTransactionId. */
	succeeded = doDispatchDtxProtocolCommand(DTX_PROTOCOL_COMMAND_RECOVERY_COMMIT_PREPARED, /* flags */ 0,
											 gid, InvalidDistributedTransactionId,
											 &badGangs, /* raiseError */ false,
											 &direct, NULL, 0);
	if (!succeeded)
	{
		elog(FATAL, "Crash recovery broadcast of the distributed transaction 'Commit Prepared' broadcast failed to one or more segments for gid = %s.", gid);
	}
	else
	{
		elog(LOG, "Crash recovery broadcast of the distributed transaction 'Commit Prepared' broadcast succeeded for gid = %s.", gid);
	}

	return succeeded;
}

static void
doAbortInDoubt(char *gid)
{
	bool		succeeded;
	bool		badGangs;

	CdbDispatchDirectDesc direct = default_dispatch_direct_desc;

	/* UNDONE: Pass real gxid instead of InvalidDistributedTransactionId. */
	succeeded = doDispatchDtxProtocolCommand(DTX_PROTOCOL_COMMAND_RECOVERY_ABORT_PREPARED, /* flags */ 0,
											 gid, InvalidDistributedTransactionId,
											 &badGangs, /* raiseError */ false,
											 &direct, NULL, 0);
	if (!succeeded)
	{
		elog(FATAL, "Crash recovery retry of the distributed transaction 'Abort Prepared' broadcast failed to one or more segments for gid = %s.  System will retry again later", gid);
	}
	else
	{
		elog(LOG, "Crash recovery broadcast of the distributed transaction 'Abort Prepared' broadcast succeeded for gid = %s", gid);
	}
}

/*
 * prepare a global transaction, called by user commit
 * or by CommitTransaction
 */
void
prepareDtxTransaction(void)
{
	if (DistributedTransactionContext != DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE)
	{
		elog(DTM_DEBUG5, "prepareDtxTransaction nothing to do (DistributedTransactionContext = '%s')",
			 DtxContextToString(DistributedTransactionContext));
		return;
	}

	if (currentGxact == NULL)
	{
		return;
	}

	if (currentGxact->state == DTX_STATE_ACTIVE_NOT_DISTRIBUTED)
	{
		/*
		 * This transaction did not go distributed.
		 */
		elog(DTM_DEBUG5, "prepareDtxTransaction ignoring not distributed gid = %s", currentGxact->gid);
		releaseGxact();
		return;
	}

	elog(DTM_DEBUG5,
		 "prepareDtxTransaction called with state = %s",
		 DtxStateToString(currentGxact->state));

	Assert(currentGxact->state == DTX_STATE_ACTIVE_DISTRIBUTED);

	/*
	 * Broadcast PREPARE TRANSACTION to segments.
	 */
	doPrepareTransaction();
}

/*
 * rollback a global transaction, called by user rollback
 * or by AbortTransaction during Postgres automatic rollback
 */
void
rollbackDtxTransaction(void)
{
	if (DistributedTransactionContext != DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE)
	{
		elog(DTM_DEBUG5, "rollbackDtxTransaction nothing to do (DistributedTransactionContext = '%s')",
			 DtxContextToString(DistributedTransactionContext));
		return;
	}
	if (currentGxact == NULL)
	{
		elog(DTM_DEBUG5, "rollbackDtxTransaction nothing to do (currentGxact == NULL)");
		return;
	}

	elog(DTM_DEBUG5, "rollbackDtxTransaction called with state = %s, gid = %s",
		 DtxStateToString(currentGxact->state), currentGxact->gid);

	switch (currentGxact->state)
	{
		case DTX_STATE_ACTIVE_NOT_DISTRIBUTED:

			/*
			 * Let go of these...
			 */
			releaseGxact();
			return;

		case DTX_STATE_ACTIVE_DISTRIBUTED:
			setCurrentGxactState(DTX_STATE_NOTIFYING_ABORT_NO_PREPARED);
			break;

		case DTX_STATE_PREPARING:
			if (currentGxact->badPrepareGangs)
			{
				setCurrentGxactState(DTX_STATE_RETRY_ABORT_PREPARED);

				/*
				 * DisconnectAndDestroyAllGangs and ResetSession happens
				 * inside retryAbortPrepared.
				 */
				retryAbortPrepared();
				releaseGxact();
				return;
			}
			setCurrentGxactState(DTX_STATE_NOTIFYING_ABORT_SOME_PREPARED);
			break;

		case DTX_STATE_PREPARED:
			setCurrentGxactState(DTX_STATE_NOTIFYING_ABORT_PREPARED);
			break;

		case DTX_STATE_NOTIFYING_ABORT_NO_PREPARED:

			/*
			 * By deallocating the gang, we will force a new gang to connect
			 * to all the segment instances.  And, we will abort the
			 * transactions in the segments.
			 */
			elog(NOTICE, "Releasing segworker groups to finish aborting the transaction.");
			DisconnectAndDestroyAllGangs(true);

			/*
			 * This call will at a minimum change the session id so we will
			 * not have SharedSnapshotAdd colissions.
			 */
			CheckForResetSession();

			releaseGxact();
			return;

		case DTX_STATE_NOTIFYING_ABORT_SOME_PREPARED:
		case DTX_STATE_NOTIFYING_ABORT_PREPARED:
			elog(FATAL, "Unable to complete the 'Abort Prepared' broadcast for gid '%s'",
				 currentGxact->gid);
			break;

		case DTX_STATE_FORCED_COMMITTED:
		case DTX_STATE_NOTIFYING_COMMIT_PREPARED:
		case DTX_STATE_INSERTING_COMMITTED:
		case DTX_STATE_INSERTED_COMMITTED:
		case DTX_STATE_INSERTING_FORGET_COMMITTED:
		case DTX_STATE_INSERTED_FORGET_COMMITTED:
		case DTX_STATE_RETRY_COMMIT_PREPARED:
		case DTX_STATE_RETRY_ABORT_PREPARED:
		case DTX_STATE_CRASH_COMMITTED:
			elog(DTM_DEBUG5, "rollbackDtxTransaction dtx state \"%s\" not expected here",
				 DtxStateToString(currentGxact->state));
			releaseGxact();
			return;

		default:
			elog(PANIC, "Unrecognized dtx state: %d",
				 (int) currentGxact->state);
			break;
	}


	Assert(currentGxact->state == DTX_STATE_NOTIFYING_ABORT_NO_PREPARED ||
		   currentGxact->state == DTX_STATE_NOTIFYING_ABORT_SOME_PREPARED ||
		   currentGxact->state == DTX_STATE_NOTIFYING_ABORT_PREPARED);

	/*
	 * if the process is in the middle of blowing up... then we don't do
	 * anything here.  we can resolve any in-doubt transactions later.
	 *
	 * We can't dispatch -- but we *do* need to free up shared-memory entries.
	 */
	if (proc_exit_inprogress)
	{
		/*
		 * Unable to complete distributed abort broadcast with possible
		 * prepared transactions...
		 */
		if (currentGxact->state == DTX_STATE_NOTIFYING_ABORT_SOME_PREPARED ||
			currentGxact->state == DTX_STATE_NOTIFYING_ABORT_PREPARED)
		{
			elog(FATAL, "Unable to complete the 'Abort Prepared' broadcast for gid '%s'",
				 currentGxact->gid);
		}

		Assert(currentGxact->state == DTX_STATE_NOTIFYING_ABORT_NO_PREPARED);

		/*
		 * By deallocating the gang, we will force a new gang to connect to
		 * all the segment instances.  And, we will abort the transactions in
		 * the segments.
		 */
		DisconnectAndDestroyAllGangs(true);

		/*
		 * This call will at a minimum change the session id so we will not
		 * have SharedSnapshotAdd colissions.
		 */
		CheckForResetSession();

		releaseGxact();
		return;
	}

	doNotifyingAbort();

	return;
}

/*
 * Error handling in initTM() is our caller.
 *
 * recoverTM() may throw errors.
 */
static void
initTM_recover_as_needed(void)
{
	Assert(shmTmRecoverred != NULL);

	/* Need to recover ? */
	if (!*shmTmRecoverred)
	{
		getTmLock();

		/* Still need to recover? */
		if (!*shmTmRecoverred)
		{
			volatile int savedInterruptHoldoffCount = InterruptHoldoffCount;

			/*
			 * We have to catch errors here, otherwise the silly TmLock will
			 * stay in the backend process until this process goes away.
			 */
			PG_TRY();
			{
				recoverTM();
				*shmTmRecoverred = true;

				/*
				 * The in-doubt transactions are recovered. Perform
				 * PersistentTable-Catalog non-database specific
				 * verifications, if requested and if needed
				 */
				if (debug_persistent_ptcat_verification &&
					Persistent_PostDTMRecv_PTCatVerificationNeeded())
				{
					Persistent_PrintHash();
					Persistent_PostDTMRecv_NonDBSpecificPTCatVerification();
				}
			}
			PG_CATCH();
			{
				/*
				 * We can't simply use HOLD_INTERRUPTS as in LWLockRelease,
				 * because at this point we don't know if other LWLocks have
				 * been acquired by myself.  Also, we don't know if
				 * releaseTmLock actually releases the lock, depending on
				 * ControlLockCount. Instead, restore the previous value,
				 * which is reset to 0 in errfinish.
				 */
				InterruptHoldoffCount = savedInterruptHoldoffCount;
				releaseTmLock();

				/* Assuming we have a catcher above... */
				PG_RE_THROW();
			}
			PG_END_TRY();
		}

		releaseTmLock();
	}
}

static char *
getSuperuser(Oid *userOid)
{
	char	   *suser = NULL;
	Relation	auth_rel;
	HeapTuple	auth_tup;
	HeapScanDesc auth_scan;
	ScanKeyData key[2];
	bool		isNull;

	ScanKeyInit(&key[0],
				Anum_pg_authid_rolsuper,
				BTEqualStrategyNumber, F_BOOLEQ,
				BoolGetDatum(true));

	ScanKeyInit(&key[1],
				Anum_pg_authid_rolcanlogin,
				BTEqualStrategyNumber, F_BOOLEQ,
				BoolGetDatum(true));

	auth_rel = heap_open(AuthIdRelationId, AccessShareLock);
	auth_scan = heap_beginscan(auth_rel, SnapshotNow, 2, key);

	while (HeapTupleIsValid(auth_tup = heap_getnext(auth_scan,
													ForwardScanDirection)))
	{
		Datum		attrName;
		Datum		attrNameOid;

		(void) heap_getattr(auth_tup, Anum_pg_authid_rolvaliduntil,
							auth_rel->rd_att, &isNull);
		/* we actually want it to be NULL, that means always valid */
		if (!isNull)
			continue;

		attrName = heap_getattr(auth_tup, Anum_pg_authid_rolname,
								auth_rel->rd_att, &isNull);

		Assert(!isNull);

		suser = pstrdup(DatumGetCString(attrName));

		attrNameOid = heap_getattr(auth_tup, ObjectIdAttributeNumber,
								   auth_rel->rd_att, &isNull);
		Assert(!isNull);
		*userOid = DatumGetObjectId(attrNameOid);

		break;
	}

	heap_endscan(auth_scan);
	heap_close(auth_rel, AccessShareLock);

	return suser;
}

static char *
ChangeToSuperuser()
{
	char	   *olduser = NULL;
	char	   *newuser;
	Oid			userOid = InvalidOid;
	MemoryContext oldcontext;

	if (!IsAuthenticatedUserSuperUser())
	{
		oldcontext = MemoryContextSwitchTo(TopMemoryContext);
		newuser = getSuperuser(&userOid);
		MemoryContextSwitchTo(oldcontext);

		olduser = MyProcPort->user_name;
		SetSessionUserId(userOid, true);
		MyProcPort->user_name = newuser;
	}

	return olduser;
}

static void
RestoreToUser(char *olduser)
{
	MemoryContext oldcontext;

	if (!IsAuthenticatedUserSuperUser())
	{
		oldcontext = MemoryContextSwitchTo(TopMemoryContext);
		pfree(MyProcPort->user_name);
		MemoryContextSwitchTo(oldcontext);

		MyProcPort->user_name = olduser;
		SetSessionUserId(GetAuthenticatedUserId(), false);
	}
}

/*
 * Initialize TM, called by cdb_setup() for each QD process.
 *
 * First call to this function will trigger tm recovery.
 *
 * MPP-9894: in 4.0, if we've been started with enough segments to
 * run, but without having them in the right "roles" (see
 * gp_segment_configuration), we need to prober to convert them -- our
 * first attempt to dispatch will fail, we've got to catch that! The
 * retry should be fine, if not we're in serious "FATAL" trouble.
 */
void
initTM(void)
{
	char	   *olduser = NULL;
	bool		exists = false;
	MemoryContext oldcontext;
	bool		succeeded,
				first;

	Assert(shmTmRecoverred != NULL);

	/* Need to recover ? */
	if (!*shmTmRecoverred)
	{
		/*
		 * DTM initialization should be done in the context of the superuser,
		 * and not the user who initiated this backend (MPP-13866). Following
		 * code changes the context to superuser and and then restores it
		 * back.
		 */
		olduser = ChangeToSuperuser();

		SIMPLE_FAULT_INJECTOR(DtmInit);

		oldcontext = CurrentMemoryContext;
		succeeded = false;
		first = true;
		while (true)
		{
			/*
			 * MPP-9894: during startup, we don't have a top-level
			 * PG_TRY/PG_CATCH block yet, the dispatcher may throw errors: we
			 * need to catch them.
			 */
			PG_TRY();
			{
				/*
				 * FtsNotifyProber could throw ERROR, so we should catch it if
				 * it happens.
				 */
				if (!first)
					FtsNotifyProber();

				initTM_recover_as_needed();
				succeeded = true;
			}
			PG_CATCH();
			{
				MemoryContextSwitchTo(oldcontext);

				elog(LOG, "DTM initialization, caught exception: "
					 "looking for failed segments.");

				/* Log the error. */
				EmitErrorReport();
				FlushErrorState();

				/*
				 * Keep going outside of PG_TRY block even if we want to
				 * retry; don't jumping out of this block without PG_END_TRY.
				 */
			}
			PG_END_TRY();

			if (!succeeded)
			{
				if (first)
				{
					first = false;
					continue;
				}
				else
				{
					elog(LOG, "DTM initialization, failed on retry.");
					elog(FATAL, "DTM initialization: failure during startup "
						 "recovery, retry failed, check segment status");
				}
			}

			Assert(!LWLockHeldByMe(shmControlLock));

			/*
			 * We are done with the recovery.
			 */
			break;
		}

		RestoreToUser(olduser);

		freeGangsForPortal(NULL);
	}
	else
	{
		PG_TRY();
		{
			/*
			 * Do Database-specific PTCat verification ? Yes, if 1- GUC is
			 * turned ON and if 2- DTM recovery INDEED happened and if 3-
			 * Current Database has not been verified before (If the Database
			 * is present in the Hash Table)
			 */
			Assert(MyDatabaseId != InvalidOid);
			if (debug_persistent_ptcat_verification &&
				Persistent_PostDTMRecv_PTCatVerificationNeeded())
			{
				getTmLock();
				Persistent_PostDTMRecv_LookupHashEntry(MyDatabaseId, &exists);
				if (exists)
				{
					olduser = ChangeToSuperuser();

					Persistent_PostDTMRecv_DBSpecificPTCatVerification();

					/*
					 * The current database is now verified. Remove its entry
					 * from the Hash table to avoid performing these
					 * verifications next time a session connects to the
					 * current database
					 */
					Persistent_PostDTMRecv_RemoveHashEntry(MyDatabaseId);

					RestoreToUser(olduser);
					freeGangsForPortal(NULL);
				}
				releaseTmLock();
			}
		}
		PG_CATCH();
		{
			elog(FATAL, " Failure during DTM Post Recovery PersistentTables-Catalog"
				 " DB-specific Verification");
		}
		PG_END_TRY();
	}
}

/* get tm share memory size */
int
tmShmemSize(void)
{
	if ((Gp_role != GP_ROLE_DISPATCH) && (Gp_role != GP_ROLE_UTILITY))
		return 0;

	return
		MAXALIGN(TMCONTROLBLOCK_BYTES(max_tm_gxacts) + max_tm_gxacts * sizeof(TMGXACT));
}


/*
 * tmShmemInit - should be called only once from postmaster and inherit by all
 * postgres processes
 */
void
tmShmemInit(void)
{
	bool		found;
	TmControlBlock *shared;

	/*
	 * max_prepared_xacts is a guc which is postmaster-startup setable -- it
	 * can only be updated by restarting the system. Global transactions will
	 * all use two-phase commit, so the number of global transactions is bound
	 * to the number of prepared.
	 */
	max_tm_gxacts = max_prepared_xacts;

	if ((Gp_role != GP_ROLE_DISPATCH) && (Gp_role != GP_ROLE_UTILITY))
		return;

	shared = (TmControlBlock *) ShmemInitStruct("Transaction manager", tmShmemSize(), &found);
	if (!shared)
		elog(FATAL, "could not initialize transaction manager share memory");

	shmControlLock = shared->ControlLock;
	shmTmRecoverred = &shared->recoverred;
	shmDistribTimeStamp = &shared->distribTimeStamp;
	shmGIDSeq = &shared->seqno;
	/* Only initialize this if we are the creator of the shared memory */
	if (!found)
	{
		time_t		t = time(NULL);

		if (t == (time_t) -1)
		{
			elog(PANIC, "cannot generate global transaction id");
		}

		*shmDistribTimeStamp = (DistributedTransactionTimeStamp) t;
		elog(DEBUG1, "DTM start timestamp %u", *shmDistribTimeStamp);

		*shmGIDSeq = FirstDistributedTransactionId;
	}
	shmDtmStarted = &shared->DtmStarted;
	shmNextSnapshotId = &shared->NextSnapshotId;
	shmNumGxacts = &shared->num_active_xacts;
	shmGxactArray = shared->gxact_array;

	if (!IsUnderPostmaster)
		/* Initialize locks and shared memory area */
	{
		int			i = 0;
		TMGXACT    *gxact = NULL;

		shared->ControlLock = LWLockAssign();
		shmControlLock = shared->ControlLock;

		/* initialize gxact array */
		gxact = (TMGXACT *) (shmGxactArray + max_tm_gxacts);
		for (i = 0; i < max_tm_gxacts; i++)
		{
			gxact->debugIndex = i;
			shmGxactArray[i] = gxact++;
		}
	}
}

/*
 * restore global transaction during tm log recovery
 */
static void
restoreGxact(TMGXACT_LOG *gxact_log, DtxState state)
{
	Assert(gxact_log != NULL);

	initGxact(currentGxact);

	/*
	 * Copy the log fields.
	 *
	 * The length check here requires the identifer have a trailing NUL
	 * character.
	 */
	if (strlen(gxact_log->gid) >= TMGIDSIZE)
		elog(PANIC, "Distribute transaction identifier too long (%d)",
			 (int) strlen(gxact_log->gid));
	memcpy(currentGxact->gid, gxact_log->gid, TMGIDSIZE);
	currentGxact->gxid = gxact_log->gxid;

	setCurrentGxactState(state);
}

/* mppTxnOptions:
 * Generates an int containing the appropriate flags to direct the remote
 * segdb QE process to perform any needed transaction commands before or
 * after the statement.
 */
int
mppTxnOptions(bool needTwoPhase)
{
	int			options = 0;

	elog(DTM_DEBUG5,
		 "mppTxnOptions DefaultXactIsoLevel = %s, DefaultXactReadOnly = %s, XactIsoLevel = %s, XactReadOnly = %s.",
		 IsoLevelAsUpperString(DefaultXactIsoLevel), (DefaultXactReadOnly ? "true" : "false"),
		 IsoLevelAsUpperString(XactIsoLevel), (XactReadOnly ? "true" : "false"));

	if (needTwoPhase)
		options |= GP_OPT_NEED_TWO_PHASE;

	if (XactIsoLevel == XACT_READ_COMMITTED)
		options |= GP_OPT_READ_COMMITTED;
	else if (XactIsoLevel == XACT_REPEATABLE_READ)
		options |= GP_OPT_REPEATABLE_READ;
	else if (XactIsoLevel == XACT_SERIALIZABLE)
		options |= GP_OPT_SERIALIZABLE;

	if (XactReadOnly)
		options |= GP_OPT_READ_ONLY;

	if (currentGxact != NULL && currentGxact->explicitBeginRemembered)
		options |= GP_OPT_EXPLICT_BEGIN;

	elog(DTM_DEBUG5,
		 "mppTxnOptions txnOptions = 0x%x, needTwoPhase = %s, explicitBegin = %s, isoLevel = %s, readOnly = %s.",
		 options,
		 (isMppTxOptions_NeedTwoPhase(options) ? "true" : "false"), (isMppTxOptions_ExplicitBegin(options) ? "true" : "false"),
		 IsoLevelAsUpperString(mppTxOptions_IsoLevel(options)), (isMppTxOptions_ReadOnly(options) ? "true" : "false"));

	return options;

}

int
mppTxOptions_IsoLevel(int txnOptions)
{
	if (txnOptions & GP_OPT_READ_COMMITTED)
		return XACT_READ_COMMITTED;
	else if (txnOptions & GP_OPT_SERIALIZABLE)
		return XACT_SERIALIZABLE;
	else if (txnOptions & GP_OPT_REPEATABLE_READ)
		return XACT_REPEATABLE_READ;
	else
		return XACT_READ_UNCOMMITTED;
}

bool
isMppTxOptions_ReadOnly(int txnOptions)
{
	return ((txnOptions & GP_OPT_READ_ONLY) != 0);
}



/* unpackMppTxnOptions:
 * Unpack an int containing the appropriate flags to direct the remote
 * segdb QE process to perform any needed transaction commands before or
 * after the statement.
 */
void
unpackMppTxnOptions(int txnOptions, int *isoLevel, bool *readOnly)
{
	*isoLevel = mppTxOptions_IsoLevel(txnOptions);

	*readOnly = isMppTxOptions_ReadOnly(txnOptions);
}

/* isMppTxOptions_StatementWantsDtxTransaction:
 * Return the NeedTwoPhase flag.
 */
bool
isMppTxOptions_NeedTwoPhase(int txnOptions)
{
	return ((txnOptions & GP_OPT_NEED_TWO_PHASE) != 0);
}

/* isMppTxOptions_ExplicitBegin:
 * Return the ExplicitBegin flag.
 */
bool
isMppTxOptions_ExplicitBegin(int txnOptions)
{
	return ((txnOptions & GP_OPT_EXPLICT_BEGIN) != 0);
}


/* acquire tm lw lock */
void
getTmLock(void)
{

	if (ControlLockCount++ == 0)
		LWLockAcquire(shmControlLock, LW_EXCLUSIVE);
}

/* release tm lw lock */
void
releaseTmLock(void)
{
	if (--ControlLockCount == 0)
		LWLockRelease(shmControlLock);

}

/*
 * Redo transaction commit log record.
 */
void
redoDtxCheckPoint(TMGXACT_CHECKPOINT *gxact_checkpoint)
{
	int			committedCount;

	int			i;

	/*
	 * For checkpoint same as REDO, lets add entries to file in utility and
	 * in-memory if Dispatch.
	 */

	committedCount = gxact_checkpoint->committedCount;
	elog(DTM_DEBUG5, "redoDtxCheckPoint has committedCount = %d", committedCount);
	if (Debug_persistent_recovery_print)
	{
		elog(PersistentRecovery_DebugPrintLevel(),
			 "redoDtxCheckPoint: committedCount = %d",
			 committedCount);
	}

	for (i = 0; i < committedCount; i++)
	{
		redoDistributedCommitRecord(&gxact_checkpoint->committedGxactArray[i]);
	}
}

static void
GetRedoFileName(char *path)
{
	snprintf(path, MAXPGPATH,
			 "%s/" UTILITYMODEDTMREDO_DIR "/" UTILITYMODEDTMREDO_FILE, DataDir);
	elog(DTM_DEBUG3, "Returning save DTM redo file path = %s", path);
}

void
UtilityModeFindOrCreateDtmRedoFile(void)
{
	char		path[MAXPGPATH];

	if (Gp_role != GP_ROLE_UTILITY)
	{
		elog(DTM_DEBUG3, "Not in Utility Mode (role = %s) -- skipping finding or creating DTM redo file",
			 role_to_string(Gp_role));
		return;
	}
	GetRedoFileName(path);

	redoFileFD = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (redoFileFD < 0)
	{
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not create save DTM redo file \"%s\"",
						path)));
	}

	redoFileOffset = lseek(redoFileFD, 0, SEEK_END);
	elog(DTM_DEBUG3, "Succesfully opened DTM redo file %s (end offset %d)",
		 path, redoFileOffset);
}

/*
 *
 */
static void
UtilityModeSaveRedo(bool committed, TMGXACT_LOG *gxact_log)
{
	TMGXACT_UTILITY_MODE_REDO utilityModeRedo;
	int			write_len;

	utilityModeRedo.committed = committed;
	memcpy(&utilityModeRedo.gxact_log, gxact_log, sizeof(TMGXACT_LOG));

	elog(DTM_DEBUG5, "Writing {committed = %s, gid = %s, gxid = %u} to DTM redo file",
		 (utilityModeRedo.committed ? "true" : "false"),
		 utilityModeRedo.gxact_log.gid,
		 utilityModeRedo.gxact_log.gxid);

	write_len = write(redoFileFD, &utilityModeRedo, sizeof(TMGXACT_UTILITY_MODE_REDO));
	if (write_len != sizeof(TMGXACT_UTILITY_MODE_REDO))
	{
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not write save DTM redo file : %m")));
	}

}

void
UtilityModeCloseDtmRedoFile(void)
{
	if (Gp_role != GP_ROLE_UTILITY)
	{
		elog(DTM_DEBUG3, "Not in Utility Mode (role = %s)-- skipping closing DTM redo file",
			 role_to_string(Gp_role));
		return;
	}
	elog(DTM_DEBUG3, "Closing DTM redo file");
	close(redoFileFD);
}

static void
ReplayRedoFromUtilityMode(void)
{
	TMGXACT_UTILITY_MODE_REDO utilityModeRedo;

	int			fd;
	int			read_len;
	int			errno;
	char		path[MAXPGPATH];
	int			entries;

	entries = 0;

	GetRedoFileName(path);

	fd = open(path, O_RDONLY, 0);
	if (fd < 0)
	{
		/* UNDONE: Distinquish "not found" from other errors. */
		elog(DTM_DEBUG3, "Could not open DTM redo file %s for reading",
			 path);
		return;
	}

	elog(DTM_DEBUG3, "Succesfully opened DTM redo file %s for reading",
		 path);

	while (true)
	{
		errno = 0;
		read_len = read(fd, &utilityModeRedo, sizeof(TMGXACT_UTILITY_MODE_REDO));

		if (read_len == 0)
			break;
		else if (read_len != sizeof(TMGXACT_UTILITY_MODE_REDO) && errno == 0)
			elog(ERROR, "Bad redo length (expected %d and found %d)",
				 (int) sizeof(TMGXACT_UTILITY_MODE_REDO), read_len);
		else if (errno != 0)
		{
			close(fd);
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("error reading DTM redo file: %m")));
		}

		elog(DTM_DEBUG5, "Read {committed = %s, gid = %s, gxid = %u} from DTM redo file",
			 (utilityModeRedo.committed ? "true" : "false"),
			 utilityModeRedo.gxact_log.gid,
			 utilityModeRedo.gxact_log.gxid);
		if (utilityModeRedo.committed)
		{
			redoDistributedCommitRecord(&utilityModeRedo.gxact_log);
		}
		else
		{
			redoDistributedForgetCommitRecord(&utilityModeRedo.gxact_log);
		}

		entries++;
	}

	elog(DTM_DEBUG5, "Processed %d entries from DTM redo file",
		 entries);
	close(fd);

}

static void
RemoveRedoUtilityModeFile(void)
{
	char		path[MAXPGPATH];
	bool		removed;

	GetRedoFileName(path);
	removed = (unlink(path) == 0);
	elog(DTM_DEBUG5, "Removed DTM redo file %s (%s)",
		 path, (removed ? "true" : "false"));
}

/*
 * Redo transaction commit log record.
 */
void
redoDistributedCommitRecord(TMGXACT_LOG *gxact_log)
{

	int			i;

	/*
	 * The length check here requires the identifer have a trailing NUL
	 * character.
	 */
	if (strlen(gxact_log->gid) >= TMGIDSIZE)
		elog(PANIC, "Distribute transaction identifier too long (%d)",
			 (int) strlen(gxact_log->gid));

	if (Gp_role == GP_ROLE_UTILITY)
	{
		elog(DTM_DEBUG3, "DB in Utility mode.  Save DTM distributed commit until later.");
		UtilityModeSaveRedo(true, gxact_log);
		return;
	}

	for (i = 0; i < *shmNumGxacts; i++)
	{
		if (strcmp(gxact_log->gid, shmGxactArray[i]->gid) == 0)
		{
			/* found an active global transaction */
			currentGxact = shmGxactArray[i];
			break;
		}
	}
	if (i == *shmNumGxacts)
	{
		/*
		 * Transaction not found, this is the first log of this transaction.
		 */
		if (*shmNumGxacts >= max_tm_gxacts)
		{
			ereport(FATAL,
					(errmsg("the limit of %d distributed transactions has been reached.",
							max_tm_gxacts),
					 errdetail("The global user configuration (GUC) server parameter max_prepared_transactions controls this limit.")));
		}

		currentGxact = shmGxactArray[(*shmNumGxacts)++];
	}

	restoreGxact(gxact_log, DTX_STATE_CRASH_COMMITTED);

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "Crash recovery redo added committed distributed transaction gid = %s", currentGxact->gid);

	elog((Debug_print_full_dtm ? INFO : DEBUG5),
		 "Crash recovery redo added committed distributed transaction gid = %s", currentGxact->gid);

	/*
	 * Don't leave the currentGxact point in-use.
	 */
	currentGxact = NULL;
}

/*
 * Redo transaction forget commit log record.
 */
void
redoDistributedForgetCommitRecord(TMGXACT_LOG *gxact_log)
{
	int			i;

	if (Gp_role == GP_ROLE_UTILITY)
	{
		elog(DTM_DEBUG3, "DB in Utility mode.  Save DTM disributed forget until later.");
		UtilityModeSaveRedo(false, gxact_log);
		return;
	}

	for (i = 0; i < *shmNumGxacts; i++)
	{
		if (strcmp(gxact_log->gid, shmGxactArray[i]->gid) == 0)
		{
			/* found an active global transaction */
			currentGxact = shmGxactArray[i];
			elog((Debug_print_full_dtm ? INFO : DEBUG5),
				 "Crash recovery redo removed committed distributed transaction gid = %s for forget",
				 currentGxact->gid);
			releaseGxact();
			return;
		}
	}

	elog((Debug_print_full_dtm ? WARNING : DEBUG5),
		 "Crash recovery redo did not find committed distributed transaction gid = %s for forget",
		 gxact_log->gid);

}

static void
descGxactLog(StringInfo buf, TMGXACT_LOG *gxact_log)
{
	appendStringInfo(buf, " gid = %s, gxid = %u",
					 gxact_log->gid, gxact_log->gxid);
}

/*
 * Describe redo transaction commit log record.
 */
void
descDistributedCommitRecord(StringInfo buf, TMGXACT_LOG *gxact_log)
{
	descGxactLog(buf, gxact_log);
}

/*
 * Describe redo transaction forget commit log record.
 */
void
descDistributedForgetCommitRecord(StringInfo buf, TMGXACT_LOG *gxact_log)
{
	descGxactLog(buf, gxact_log);
}


/*=========================================================================
 * HELPER FUNCTIONS
 */

static bool
doDispatchDtxProtocolCommand(DtxProtocolCommand dtxProtocolCommand, int flags,
							 char *gid, DistributedTransactionId gxid,
							 bool *badGangs, bool raiseError,
							 CdbDispatchDirectDesc *direct,
							 char *serializedDtxContextInfo,
							 int serializedDtxContextInfoLen)
{
	int			i,
				resultCount,
				numOfFailed = 0;

	char	   *dtxProtocolCommandStr = 0;

	struct pg_result **results = NULL;
	StringInfoData errbuf;

	dtxProtocolCommandStr = DtxProtocolCommandToString(dtxProtocolCommand);

	if (Test_print_direct_dispatch_info)
	{
		if (direct->directed_dispatch)
			elog(INFO, "Distributed transaction command '%s' to SINGLE content", dtxProtocolCommandStr);
		else
			elog(INFO, "Distributed transaction command '%s' to ALL contents", dtxProtocolCommandStr);
	}
	elog(DTM_DEBUG5,
		 "dispatchDtxProtocolCommand: %d ('%s'), direct content #: %d",
		 dtxProtocolCommand, dtxProtocolCommandStr,
		 direct->directed_dispatch ? direct->content[0] : -1);

	initStringInfo(&errbuf);
	results = CdbDispatchDtxProtocolCommand(dtxProtocolCommand, flags,
											dtxProtocolCommandStr,
											gid, gxid,
											&errbuf, &resultCount, badGangs, direct,
											serializedDtxContextInfo, serializedDtxContextInfoLen);

	if (errbuf.len > 0)
	{
		ereport((raiseError ? ERROR : LOG),
				(errmsg("DTM error (gathered results from cmd '%s')", dtxProtocolCommandStr),
				 errdetail("%s", errbuf.data)));
		return false;
	}

	if (results == NULL)
	{
		numOfFailed++;			/* If we got no results, we need to treat it
								 * as an error! */
	}

	for (i = 0; i < resultCount; i++)
	{
		char	   *cmdStatus;
		ExecStatusType resultStatus;

		/*
		 * note: PQresultStatus() is smart enough to deal with results[i] ==
		 * NULL
		 */
		resultStatus = PQresultStatus(results[i]);
		if (resultStatus != PGRES_COMMAND_OK &&
			resultStatus != PGRES_TUPLES_OK)
		{
			numOfFailed++;
		}
		else
		{
			/*
			 * success ? If an error happened during a transaction which
			 * hasn't already been caught when we try a prepare we'll get a
			 * rollback from our prepare ON ONE SEGMENT: so we go look at the
			 * status, otherwise we could issue a COMMIT when we don't want
			 * to!
			 */
			cmdStatus = PQcmdStatus(results[i]);

			elog(DEBUG3, "DTM: status message cmd '%s' [%d] result '%s'", dtxProtocolCommandStr, i, cmdStatus);
			if (strncmp(cmdStatus, dtxProtocolCommandStr, strlen(cmdStatus)) != 0)
			{
				/* failed */
				numOfFailed++;
			}
		}
	}

	/* discard the errbuf text */
	pfree(errbuf.data);

	for (i = 0; i < resultCount; i++)
		PQclear(results[i]);

	if (results)
		free(results);

	return (numOfFailed == 0);
}


bool
dispatchDtxCommand(const char *cmd)
{
	int			i,
				numOfFailed = 0;

	CdbPgResults cdb_pgresults = {NULL, 0};

	elog(DTM_DEBUG5, "dispatchDtxCommand: '%s'", cmd);

	CdbDispatchCommand(cmd, DF_NONE, &cdb_pgresults);

	if (cdb_pgresults.numResults == 0)
	{
		return false;			/* If we got no results, we need to treat it
								 * as an error! */
	}

	for (i = 0; i < cdb_pgresults.numResults; i++)
	{
		char	   *cmdStatus;
		ExecStatusType resultStatus;

		/*
		 * note: PQresultStatus() is smart enough to deal with results[i] ==
		 * NULL
		 */
		resultStatus = PQresultStatus(cdb_pgresults.pg_results[i]);
		if (resultStatus != PGRES_COMMAND_OK &&
			resultStatus != PGRES_TUPLES_OK)
		{
			numOfFailed++;
		}
		else
		{
			/*
			 * success ? If an error happened during a transaction which
			 * hasn't already been caught when we try a prepare we'll get a
			 * rollback from our prepare ON ONE SEGMENT: so we go look at the
			 * status, otherwise we could issue a COMMIT when we don't want
			 * to!
			 */
			cmdStatus = PQcmdStatus(cdb_pgresults.pg_results[i]);

			elog(DEBUG3, "DTM: status message cmd '%s' [%d] result '%s'", cmd, i, cmdStatus);
			if (strncmp(cmdStatus, cmd, strlen(cmdStatus)) != 0)
			{
				/* failed */
				numOfFailed++;
			}
		}
	}

	cdbdisp_clearCdbPgResults(&cdb_pgresults);

	return (numOfFailed == 0);
}

/* initialize a global transaction context */
static void
initGxact(TMGXACT *gxact)
{
	MemSet(gxact->gid, 0, TMGIDSIZE);
	gxact->gxid = InvalidDistributedTransactionId;
	setGxactState(gxact, DTX_STATE_NONE);

	/*
	 * Memory only fields.
	 */

	gxact->sessionId = gp_session_id;

	gxact->explicitBeginRemembered = false;

	gxact->xminDistributedSnapshot = InvalidDistributedTransactionId;

	gxact->badPrepareGangs = false;

	gxact->directTransaction = false;
	gxact->directTransactionContentId = 0;
}

void
getAllDistributedXactStatus(TMGALLXACTSTATUS **allDistributedXactStatus)
{
	TMGALLXACTSTATUS *all;
	int			count;

	all = palloc(sizeof(TMGALLXACTSTATUS));
	all->next = 0;
	all->count = 0;
	all->statusArray = NULL;

	if (shmDtmStarted != NULL && *shmDtmStarted)
	{
		getTmLock();
		count = *shmNumGxacts;
		if (count > 0)
		{
			int			i;

			all->statusArray =
				palloc(MAXALIGN(count * sizeof(TMGXACTSTATUS)));
			for (i = 0; i < count; i++)
			{
				TMGXACT    *gxact = shmGxactArray[i];

				all->statusArray[i].gxid = gxact->gxid;
				if (strlen(gxact->gid) >= TMGIDSIZE)
					elog(PANIC, "Distribute transaction identifier too long (%d)",
						 (int) strlen(gxact->gid));
				memcpy(all->statusArray[i].gid, gxact->gid, TMGIDSIZE);
				all->statusArray[i].state = gxact->state;
				all->statusArray[i].sessionId = gxact->sessionId;
				all->statusArray[i].xminDistributedSnapshot = gxact->xminDistributedSnapshot;
			}

			all->count = count;
		}

		releaseTmLock();

	}

	*allDistributedXactStatus = all;
}

bool
getNextDistributedXactStatus(TMGALLXACTSTATUS *allDistributedXactStatus, TMGXACTSTATUS **distributedXactStatus)
{
	if (allDistributedXactStatus->next >= allDistributedXactStatus->count)
	{
		return false;
	}

	*distributedXactStatus = &allDistributedXactStatus->statusArray[allDistributedXactStatus->next];
	allDistributedXactStatus->next++;

	return true;
}

/*
 * DistributedSnapshotMappedEntry_Compare: A compare function for
 * DistributedTransactionId for use with qsort.
 */
static int
DistributedSnapshotMappedEntry_Compare(const void *p1, const void *p2)
{
	const DistributedTransactionId distribXid1 = *(DistributedTransactionId *) p1;
	const DistributedTransactionId distribXid2 = *(DistributedTransactionId *) p2;

	if (distribXid1 == distribXid2)
		return 0;
	else if (distribXid1 > distribXid2)
		return 1;
	else
		return -1;
}

bool
CreateDistributedSnapshot(DistributedSnapshotWithLocalMapping *distribSnapshotWithLocalMapping)
{
	int			globalCount;
	int			i;
	TMGXACT    *gxact_candidate;
	DtxState	state;
	int			count;
	DistributedTransactionId xmin;
	DistributedTransactionId xmax;
	DistributedTransactionId inProgressXid;
	DistributedSnapshotId distribSnapshotId;
	DistributedTransactionId globalXminDistributedSnapshots;
	DistributedSnapshot *ds;

	if (currentGxact == NULL)
	{
		elog(DTM_DEBUG5, "CreateDistributedSnapshot found currentGxact is NULL");
		return false;
	}

	xmin = LastDistributedTransactionId;

	/*
	 * This is analogous to the code in GetSnapshotData() (which calls
	 * ReadNewTransactionId(), the distributed-xmax of a transaction is the
	 * last distributed-xmax available
	 */
	xmax = getMaxDistributedXid();

	/*
	 * initialize for calculation with xmax, the calculation for this is on
	 * same lines as globalxmin for local snapshot.
	 */
	globalXminDistributedSnapshots = xmax;
	count = 0;
	ds = &distribSnapshotWithLocalMapping->ds;

	getTmLock();

	/*
	 * Gather up current in-progress global transactions for the distributed
	 * snapshot.
	 */
	globalCount = *shmNumGxacts;

	for (i = 0; i < globalCount; i++)
	{
		DistributedTransactionId dxid;

		gxact_candidate = shmGxactArray[i];

		state = gxact_candidate->state;
		switch (state)
		{
			case DTX_STATE_ACTIVE_NOT_DISTRIBUTED:
			case DTX_STATE_ACTIVE_DISTRIBUTED:
			case DTX_STATE_PREPARING:
			case DTX_STATE_PREPARED:
			case DTX_STATE_INSERTED_COMMITTED:
			case DTX_STATE_FORCED_COMMITTED:
			case DTX_STATE_NOTIFYING_COMMIT_PREPARED:
			case DTX_STATE_NOTIFYING_ABORT_NO_PREPARED:
			case DTX_STATE_NOTIFYING_ABORT_SOME_PREPARED:
			case DTX_STATE_NOTIFYING_ABORT_PREPARED:
			case DTX_STATE_RETRY_COMMIT_PREPARED:
			case DTX_STATE_RETRY_ABORT_PREPARED:

				/*
				 * Active or commit/abort not complete.  Keep this transaction
				 * considered for distributed snapshots.
				 */
				break;

			case DTX_STATE_INSERTED_FORGET_COMMITTED:
				elog(FATAL, "Should not see this transitional state with TM lock held -- dtx state \"%s\" not expected here",
					 DtxStateToString(state));
				break;

			case DTX_STATE_INSERTING_COMMITTED:
				elog(FATAL, "Cannot also be inserting COMMITTED into log buffer from another process with TM lock held");
				break;

			case DTX_STATE_INSERTING_FORGET_COMMITTED:
				elog(FATAL, "Cannot also be inserting FORGET COMMITTED into log buffer from another process with TM lock held");
				break;

			case DTX_STATE_CRASH_COMMITTED:

				/*
				 * From a previous system incarnation.
				 */
				continue;

			default:
				elog(PANIC, "Unexpected dtm state: %d",
					 (int) state);
				break;
		}

		/* Update globalXminDistributedSnapshots to be the smallest valid dxid */
		dxid = gxact_candidate->xminDistributedSnapshot;
		if ((dxid != InvalidDistributedTransactionId) &&
			dxid < globalXminDistributedSnapshots)
		{
			globalXminDistributedSnapshots = dxid;
		}

		/*
		 * Include the current distributed transaction in the min/max
		 * calculation.
		 */
		inProgressXid = gxact_candidate->gxid;
		if (inProgressXid < xmin)
		{
			xmin = inProgressXid;
		}
		if (inProgressXid > xmax)
		{
			xmax = inProgressXid;
		}

		if (gxact_candidate == currentGxact)
			continue;

		if (count >= ds->maxCount)
			elog(ERROR, "Too many distributed transactions for snapshot");

		ds->inProgressXidArray[count] = inProgressXid;

		count++;

		elog(DTM_DEBUG5,
			 "CreateDistributedSnapshot added inProgressDistributedXid = %u to snapshot",
			 ds->inProgressXidArray[count]);
	}

	distribSnapshotId = (*shmNextSnapshotId)++;
	releaseTmLock();

	/*
	 * Above globalXminDistributedSnapshots was calculated based on lowest
	 * dxid in all snapshots but update it to also include actual process
	 * dxids.
	 */
	if (xmin < globalXminDistributedSnapshots)
		globalXminDistributedSnapshots = xmin;

	/*
	 * Sort the entry {distribXid} to support the QEs doing culls on their
	 * DisribToLocalXact sorted lists.
	 */
	qsort(
		  ds->inProgressXidArray,
		  count,
		  sizeof(DistributedTransactionId),
		  DistributedSnapshotMappedEntry_Compare);

	/*
	 * Copy the information we just captured under lock and then sorted into
	 * the distributed snapshot.
	 */
	ds->distribTransactionTimeStamp = *shmDistribTimeStamp;
	ds->xminAllDistributedSnapshots = globalXminDistributedSnapshots;
	ds->distribSnapshotId = distribSnapshotId;
	ds->xmin = xmin;
	ds->xmax = xmax;
	ds->count = count;

	if (xmin < currentGxact->xminDistributedSnapshot)
		currentGxact->xminDistributedSnapshot = xmin;

	elog(DTM_DEBUG5,
		 "CreateDistributedSnapshot distributed snapshot has xmin = %u, count = %u, xmax = %u.",
		 xmin, count, xmax);
	elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),
		 "[Distributed Snapshot #%u] *Create* (gxid = %u, '%s')",
		 distribSnapshotId,
		 currentGxact->gxid,
		 DtxContextToString(DistributedTransactionContext));

	/*
	 * At snapshot creation time, local xid cache is empty. Gets populated as
	 * reverse mapping takes place during visibility checks using this
	 * snapshot.
	 */
	distribSnapshotWithLocalMapping->currentLocalXidsCount = 0;
	distribSnapshotWithLocalMapping->minCachedLocalXid = InvalidTransactionId;
	distribSnapshotWithLocalMapping->maxCachedLocalXid = InvalidTransactionId;

	Assert(distribSnapshotWithLocalMapping->maxLocalXidsCount != 0);
	Assert(distribSnapshotWithLocalMapping->inProgressMappedLocalXids != NULL);

	memset(distribSnapshotWithLocalMapping->inProgressMappedLocalXids,
		   InvalidTransactionId,
		   sizeof(TransactionId) * distribSnapshotWithLocalMapping->maxLocalXidsCount);

	return true;
}

/*
 * Create a global transaction context from share memory.
 */
void
createDtx(DistributedTransactionId *distribXid)
{
	TMGXACT    *gxact;

	MIRRORED_LOCK_DECLARE;

	/*
	 * Global locking order: ProcArrayLock then DTM lock.
	 */
	MIRRORED_LOCK;

	LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

	getTmLock();

	if (*shmNumGxacts >= max_tm_gxacts)
	{
		dumpAllDtx();
		releaseTmLock();
		LWLockRelease(ProcArrayLock);
		ereport(FATAL,
				(errmsg("the limit of %d distributed transactions has been reached.",
						max_tm_gxacts),
				 errdetail("The global user configuration (GUC) server parameter max_prepared_transactions controls this limit.")));
	}

	gxact = shmGxactArray[(*shmNumGxacts)++];
	initGxact(gxact);
	generateGID(gxact->gid, &gxact->gxid);

	*distribXid = gxact->gxid;

	/*
	 * Until we get our first distributed snapshot, we use our distributed
	 * transaction identifier for the minimum.
	 */
	gxact->xminDistributedSnapshot = gxact->gxid;

	setGxactState(gxact, DTX_STATE_ACTIVE_NOT_DISTRIBUTED);

	releaseTmLock();

	LWLockRelease(ProcArrayLock);

	MIRRORED_UNLOCK;

	currentGxact = gxact;

	elog(DTM_DEBUG5,
		 "createDtx created new distributed transaction gid = %s, gxid = %u.",
		 currentGxact->gid, currentGxact->gxid);
}


/*
 * Release global transaction's shared memory.
 * Must already hold ProcArrayLock and the DTM lock.
 */
static void
releaseGxact_UnderLocks(void)
{
	int			i;
	int			curr;

	if (currentGxact == NULL)
	{
		elog(FATAL, "releaseGxact expected currentGxact to not be NULL");
	}

	elog(DTM_DEBUG5,
		 "releaseGxact called for gid = %s (index = %d)",
		 currentGxact->gid, currentGxact->debugIndex);

	/* find slot of current transaction */
	curr = *shmNumGxacts;		/* A bad value we can safely test. */
	for (i = 0; i < *shmNumGxacts; i++)
	{
		if (shmGxactArray[i] == currentGxact)
		{
			curr = i;
			break;
		}
	}

	/* move this to the next available slot */
	(*shmNumGxacts)--;
	if (curr != *shmNumGxacts)
	{
		shmGxactArray[curr] = shmGxactArray[*shmNumGxacts];
		shmGxactArray[*shmNumGxacts] = currentGxact;
	}

	currentGxact = NULL;
}

/*
 * Release global transaction's shared memory.
 */
static void
releaseGxact(void)
{
	/*
	 * Global locking order: ProcArrayLock then DTM lock.
	 */
	LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

	getTmLock();

	releaseGxact_UnderLocks();

	releaseTmLock();

	LWLockRelease(ProcArrayLock);
}

/*
 * Get lock that serializes commits with DTM checkpoint info.
 * Change state to DTX_STATE_INSERTING_COMMITTED.
 */
void
insertingDistributedCommitted(void)
{
	elog(DTM_DEBUG5,
		 "insertingDistributedCommitted entering in state = %s",
		 DtxStateToString(currentGxact->state));

	getTmLock();
	Assert(currentGxact->state == DTX_STATE_PREPARED);
	setCurrentGxactState(DTX_STATE_INSERTING_COMMITTED);
}

/*
 * Change state to DTX_STATE_INSERTED_COMMITTED.
 * Release lock.
 */
void
insertedDistributedCommitted(void)
{
	elog(DTM_DEBUG5,
		 "insertedDistributedCommitted entering in state = %s for gid = %s",
		 DtxStateToString(currentGxact->state), currentGxact->gid);

	Assert(currentGxact->state == DTX_STATE_INSERTING_COMMITTED);
	setCurrentGxactState(DTX_STATE_INSERTED_COMMITTED);
	releaseTmLock();
}


/*
 * Change state to DTX_STATE_FORCED_COMMITTED.
 */
void
forcedDistributedCommitted(XLogRecPtr *recptr)
{
	elog(DTM_DEBUG5,
		 "forcedDistributedCommitted entering in state = %s for gid = %s (xlog record %X/%X)",
		 DtxStateToString(currentGxact->state), currentGxact->gid, recptr->xlogid, recptr->xrecoff);

	getTmLock();
	Assert(currentGxact->state == DTX_STATE_INSERTED_COMMITTED);
	setCurrentGxactState(DTX_STATE_FORCED_COMMITTED);
	releaseTmLock();
}



/*
 * Get check point information
 *
 * Whether DTM started or not, we must always store DTM information in
 * this checkpoint record.  A possible case to consider is we might have
 * in-progress global transactions in shared memory after postmaster reset,
 * and shutting down without performing DTM recovery.  The subsequent
 * recovery after this shutdown will read this checkpoint, so we would
 * lose the in-progress global transaction information if we didn't write it
 * here.  Note we will certainly read this global transaction information
 * even if this is a clean shutdown (i.e. not performing multi-pass recovery.)
 */
void
getDtxCheckPointInfoAndLock(char **result, int *result_size)
{
	TMGXACT    *gxact;
	DtxState	state;
	int			n;
	TMGXACT_CHECKPOINT *gxact_checkpoint;
	TMGXACT_LOG *gxact_log_array;
	int			i;
	int			actual;
	TMGXACT_LOG *gxact_log;

	getTmLock();				/* We will return with lock held below. */
	n = *shmNumGxacts;

	gxact_checkpoint = palloc(TMGXACT_CHECKPOINT_BYTES(n));
	gxact_log_array = &gxact_checkpoint->committedGxactArray[0];

	actual = 0;
	for (i = 0; i < n; i++)
	{
		gxact = shmGxactArray[i];

		gxact_log = &gxact_log_array[actual];

		state = gxact->state;
		switch (state)
		{
			case DTX_STATE_ACTIVE_NOT_DISTRIBUTED:
			case DTX_STATE_ACTIVE_DISTRIBUTED:
			case DTX_STATE_PREPARING:
			case DTX_STATE_PREPARED:

				/*
				 * Active or attempting to commit -- ignore.
				 */
				continue;

			case DTX_STATE_INSERTED_FORGET_COMMITTED:
				elog(FATAL, "Should not see this transitional state with TM lock held -- dtx state \"%s\" not expected here",
					 DtxStateToString(state));
				continue;

			case DTX_STATE_INSERTING_COMMITTED:
				elog(FATAL, "Cannot also be buffering COMMITTED from another process with TM lock held");
				continue;

			case DTX_STATE_INSERTING_FORGET_COMMITTED:
				elog(FATAL, "Cannot also be buffering FORGET COMMITTED from another process with TM lock held");
				continue;

			case DTX_STATE_INSERTED_COMMITTED:
			case DTX_STATE_FORCED_COMMITTED:
			case DTX_STATE_NOTIFYING_COMMIT_PREPARED:
			case DTX_STATE_RETRY_COMMIT_PREPARED:
			case DTX_STATE_CRASH_COMMITTED:
				break;

			case DTX_STATE_NOTIFYING_ABORT_NO_PREPARED:
			case DTX_STATE_NOTIFYING_ABORT_SOME_PREPARED:
			case DTX_STATE_NOTIFYING_ABORT_PREPARED:
			case DTX_STATE_RETRY_ABORT_PREPARED:

				/*
				 * We don't checkpoint abort.
				 */
				continue;

			default:
				elog(PANIC, "Unexpected dtm state: %d",
					 (int) state);
				break;
		}

		if (strlen(gxact->gid) >= TMGIDSIZE)
			elog(PANIC, "Distribute transaction identifier too long (%d)",
				 (int) strlen(gxact->gid));
		memcpy(gxact_log->gid, gxact->gid, TMGIDSIZE);
		gxact_log->gxid = gxact->gxid;

		elog(DTM_DEBUG5, "Add DTM checkpoint entry gid = %s.",
			 gxact->gid);
		if (Debug_persistent_recovery_print)
		{
			SUPPRESS_ERRCONTEXT_DECLARE;

			SUPPRESS_ERRCONTEXT_PUSH();

			elog(PersistentRecovery_DebugPrintLevel(),
				 "getDtxCheckPointInfoAndLock[%d]: distributed transaction identifier %s (distributed xid %u)",
				 actual,
				 gxact_log->gid,
				 gxact_log->gxid);

			SUPPRESS_ERRCONTEXT_POP();
		}

		actual++;
	}

	gxact_checkpoint->committedCount = actual;

	*result = (char *) gxact_checkpoint;
	*result_size = TMGXACT_CHECKPOINT_BYTES(actual);

	/* Return with lock held. */

	elog(DTM_DEBUG5, "Filled in DTM checkpoint information (count = %d).",
		 actual);
}

void
			freeDtxCheckPointInfoAndUnlock(char *info, int info_size __attribute__((unused)), XLogRecPtr *recptr)
{
	pfree(info);

	releaseTmLock();

	elog(DTM_DEBUG5, "Checkpoint with DTM information written at %X/%X.",
		 recptr->xlogid, recptr->xrecoff);
}

/* generate global transaction id */
static void
generateGID(char *gid, DistributedTransactionId *gxid)
{
	/* tm lock acquired by caller */
	if (*shmGIDSeq >= LastDistributedTransactionId)
	{
		releaseTmLock();
		ereport(FATAL,
				(errmsg("reached limit of %u global transactions per start", LastDistributedTransactionId)));
	}
	Assert(*shmDistribTimeStamp != 0);

	*gxid = ++(*shmGIDSeq);
	sprintf(gid, "%u-%.10u", *shmDistribTimeStamp, (*gxid));
	if (strlen(gid) >= TMGIDSIZE)
		elog(PANIC, "Distribute transaction identifier too long (%d)",
			 (int) strlen(gid));

	Assert(*gxid != InvalidDistributedTransactionId);
}

/*
 * Return the highest global transaction id that has been generated.
 */
DistributedTransactionId
getMaxDistributedXid(void)
{
	if (!shmGIDSeq)
		return 0;

	return *shmGIDSeq;
}

/*
 * recoverTM:
 * perform TM recovery, this connects to all QE and resolve all in-doubt txn.
 *
 * This gets called when there is not any other DTM activity going on.
 *
 * First, we'll replay the dtm log and get our shmem as up to date as possible
 * in order to help resolve in-doubt transactions.	Then we'll go through and
 * try and resolve in-doubt transactions based on information in the DTM log.
 * The remaining in-doubt transactions that remain (ones the DTM doesn't know
 * about) are all ABORTed.
 *
 * If we're in read-only mode; we need to get started, but we can't run the
 * full recovery. So we go get the highest distributed-xid, but don't run
 * the recovery.
 */
static void
recoverTM(void)
{
	bool		dtmRecoveryDeferred;

	/* intialize fts sync count */
	verifyFtsSyncCount();

	elog(DTM_DEBUG3, "Starting to Recover DTM...");

	/*
	 * do not do recovery if read only mode is set. in this case, there may be
	 * in-doubt transaction in down segdb , which will not be resolved at this
	 * time.
	 */
	if (isFtsReadOnlySet())
	{
		elog(DTM_DEBUG3, "FTS is Read Only.  Defer DTM recovery till later.");
		if (currentGxact != NULL)
		{
			elog(DTM_DEBUG5,
				 "recoverTM setting currentGxact to NULL for gid = %s (index = %d)",
				 currentGxact->gid, currentGxact->debugIndex);
		}
		currentGxact = NULL;

		dtmRecoveryDeferred = true;
	}
	else
		dtmRecoveryDeferred = false;

	if (Gp_role == GP_ROLE_UTILITY)
	{
		elog(DTM_DEBUG3, "DB in Utility mode.  Defer DTM recovery till later.");
		return;
	}

	if (!dtmRecoveryDeferred)
	{
		/*
		 * Attempt to recover all in-doubt transactions.
		 *
		 * first resolve all in-doubt transactions from the DTM's perspective
		 * and then resolve any remaining in-doubt transactions that the RMs
		 * have.
		 */
		recoverInDoubtTransactions();
	}
	else
		elog(LOG, "DTM starting in readonly-mode: deferring recovery");

	/* finished recovery successfully. */

	*shmGIDSeq = 1;

	*shmDtmStarted = true;
	elog(LOG, "DTM Started");
}

/* recoverInDoubtTransactions:
 * Go through all in-doubt transactions that the DTM knows about and
 * resolve them.
 */
static bool
recoverInDoubtTransactions(void)
{
	int			i;
	HTAB	   *htab;
	TMGXACT    *saved_currentGxact;

	elog(DTM_DEBUG3, "recover in-doubt distributed transactions");

	ReplayRedoFromUtilityMode();

	/*
	 * For each committed transaction found in the redo pass that was not
	 * matched by a forget committed record, change its state indicating
	 * committed notification needed.  Attempt a notification.
	 */
	elog(DTM_DEBUG5,
		 "Going to retry commit notification for distributed transactions (count = %d)",
		 *shmNumGxacts);
	dumpAllDtx();

	saved_currentGxact = currentGxact;

	for (i = 0; i < *shmNumGxacts;)
	{
		TMGXACT    *gxact = shmGxactArray[i];

		/*
		 * MPP-4867: if we are running deferred, skip any transaction we're
		 * already inside.
		 */
		if (saved_currentGxact != NULL && gxact == saved_currentGxact)
		{
			i++;
			continue;
		}
		else if (gxact->state == DTX_STATE_ACTIVE_NOT_DISTRIBUTED)
		{
			/* should take care of other sessions. */
			i++;
			continue;
		}

		if (gxact->state != DTX_STATE_CRASH_COMMITTED)
		{
			dumpAllDtx();
			elog(PANIC,
				 "recoverInDoubtTransactions found transaction in '%s' state and was expecting it to be in '%s' state",
				 DtxStateToString(gxact->state), DtxStateToString(DTX_STATE_CRASH_COMMITTED));
		}

		elog(DTM_DEBUG5,
			 "Recovering committed distributed transaction gid = %s",
			 gxact->gid);

		/*
		 * Can't start system if we cannot deliiver commits.
		 */
		doNotifyCommittedInDoubt(gxact->gid);

		/*
		 * This means there was atleast one in-doubt transactions for whom
		 * prepared commit was sent out. Hence, we've a chance to perform
		 * PT-Catalog verification if requested.
		 */
		if (debug_persistent_ptcat_verification)
			Persistent_Set_PostDTMRecv_PTCatVerificationNeeded();

		currentGxact = gxact;

		/*
		 * Global locking order: ProcArrayLock then DTM lock since calls
		 * doInsertForgetCommitted calls releaseGxact.
		 */
		LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

		getTmLock();

		/*
		 * This routine would call releaseGxact_UnderLocks, which would
		 * decrease *shmNumGxacts and do a swap, so no need to increase i
		 */
		doInsertForgetCommitted();

		releaseTmLock();

		LWLockRelease(ProcArrayLock);

	}

	currentGxact = saved_currentGxact;
	dumpAllDtx();

	/*
	 * UNDONE: Thus, any in-doubt transctions found will be for aborted
	 * transactions. UNDONE: Gather in-boubt transactions and issue aborts.
	 */
	htab = gatherRMInDoubtTransactions();

	/*
	 * go through and resolve any remaining in-doubt transactions that the
	 * RM's have AFTER recoverDTMInDoubtTransactions.  ALL of these in doubt
	 * transactions will be ABORT'd.  The fact that the DTM doesn't know about
	 * them means that something bad happened before everybody voted to
	 * COMMIT.
	 */
	abortRMInDoubtTransactions(htab);

	/*
	 * If there were any In-Doubt transactions collected from the segment
	 * instances and if the verification checks are requested then make the
	 * flag green so that the system is informed to run the verification
	 * checks post DTM is recovered
	 */
	if (debug_persistent_ptcat_verification && htab)
		Persistent_Set_PostDTMRecv_PTCatVerificationNeeded();

	/* get rid of the hashtable */
	hash_destroy(htab);

	/* yes... we are paranoid and will double check */
	htab = gatherRMInDoubtTransactions();

	/*
	 * Hmm.  we still have some remaining indoubt transactions.  For now we
	 * dont have an automated way to clean this mess up.  So we'll have to
	 * rely on smart Admins to do the job manually.  We'll error out of here
	 * and try and provide as much info as possible.
	 *
	 * TODO: We really want to be able to say this particular segdb has these
	 * remaining in-doubt transactions.
	 */
	if (htab != NULL)
	{
		StringInfoData indoubtBuff;

		initStringInfo(&indoubtBuff);

		dumpAllDtx();
		dumpRMOnlyDtx(htab, &indoubtBuff);

		ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
						errmsg("DTM Log recovery failed.  There are still unresolved "
							   "in-doubt transactions on some of the segment databaes "
							   "that were not able to be resolved for an unknown reason. "),
						errdetail("Here is a list of in-doubt transactions in the system: %s",
								  indoubtBuff.data),
						errhint("Try restarting the Greenplum Database array.  If the problem persists "
								" an Administrator will need to resolve these transactions "
								" manually.")));

	}

	RemoveRedoUtilityModeFile();

	return true;
}

/*
 * gatherRMInDoubtTransactions:
 * Builds a hashtable of all of the in-doubt transactions that exist on the
 * segment databases.  The hashtable basically just serves as a single list
 * without duplicates of all the in-doubt transactions.  It does not keep track
 * of which seg db's have which transactions in-doubt.  It currently doesn't
 * need to due to the way we handle this information later.
 */
static HTAB *
gatherRMInDoubtTransactions(void)
{
	CdbPgResults cdb_pgresults = {NULL, 0};
	const char *cmdbuf = "select gid from pg_prepared_xacts";
	PGresult   *rs;

	InDoubtDtx *lastDtx = NULL;

	HASHCTL		hctl;
	HTAB	   *htab = NULL;
	int			i;
	int			j,
				rows;
	bool		found;

	/* call to all QE to get in-doubt transactions */
	CdbDispatchCommand(cmdbuf, DF_NONE, &cdb_pgresults);

	/* If any result set is nonempty, there are in-doubt transactions. */
	for (i = 0; i < cdb_pgresults.numResults; i++)
	{
		rs = cdb_pgresults.pg_results[i];
		rows = PQntuples(rs);

		for (j = 0; j < rows; j++)
		{
			char	   *gid;

			/*
			 * we dont setup our hashtable until we know we have at least one
			 * in doubt transaction
			 */
			if (htab == NULL)
			{

				/* setup a hash table */
				hctl.keysize = TMGIDSIZE;	/* GID */
				hctl.entrysize = sizeof(InDoubtDtx);

				htab = hash_create("InDoubtDtxHash", 10, &hctl, HASH_ELEM);

				if (htab == NULL)
				{
					ereport(FATAL, (errcode(ERRCODE_OUT_OF_MEMORY),
									errmsg("DTM could not allocate hash table for InDoubtDtxList.")));
				}
			}

			gid = PQgetvalue(rs, j, 0);

			/* Now we can add entry to hash table */
			lastDtx = (InDoubtDtx *) hash_search(htab, gid, HASH_ENTER, &found);

			/*
			 * only need to bother doing work if there isn't already an entry
			 * for our GID
			 */
			if (!found)
			{
				elog(DEBUG3, "Found in-doubt transaction with GID: %s on remote RM", gid);

				strcpy(lastDtx->gid, gid);
			}

		}
	}

	cdbdisp_clearCdbPgResults(&cdb_pgresults);

	return htab;
}

static bool
FindRetryNotifyCommitted(char *gid, TMGXACT **gxact)
{
	int			i;

	for (i = 0; i < *shmNumGxacts; i++)
	{
		if (strcmp(gid, shmGxactArray[i]->gid) == 0)
		{
			/* found an retry notify committed */
			*gxact = shmGxactArray[i];
			return true;
		}
	}

	return false;
}

/*
 * abortRMInDoubtTransactions:
 * Goes through all the InDoubtDtx's in the provided htab and ABORTs them
 * across all of the QEs by sending a ROLLBACK PREPARED.
 *
 */
static void
abortRMInDoubtTransactions(HTAB *htab)
{
	HASH_SEQ_STATUS status;
	InDoubtDtx *entry = NULL;
	TMGXACT    *gxact = NULL;

	if (htab == NULL)
		return;

	/*
	 * now we have a nice hashtable full of in-doubt dtx's that we need to
	 * resolve.  so we'll use a nice big hammer to get this job done.  instead
	 * of keeping track of which QEs have a prepared txn to be aborted and
	 * which ones don't.  we just issue a ROLLBACK to all of them and ignore
	 * any pesky errors.  This is certainly not an elegant solution but is OK
	 * for now.
	 */
	hash_seq_init(&status, htab);


	while ((entry = (InDoubtDtx *) hash_seq_search(&status)) != NULL)
	{
		if (FindRetryNotifyCommitted(entry->gid, &gxact))
		{
			elog(DTM_DEBUG5, "Skipping in-doubt transaction gid = %s since it is \"%s\"",
				 gxact->gid, DtxStateToString(gxact->state));
			continue;
		}

		elog(DTM_DEBUG3, "Aborting in-doubt transaction with gid = %s", entry->gid);

		doAbortInDoubt(entry->gid);

	}
}

/*
 * dumpAllDtx:
 * used to log the current state (according to the DTM) of all the
 * global distributed transactions that are still active (in shmem).
 */
static void
dumpAllDtx(void)
{
	int			i;

	elog(LOG, "dumping all global transactions: ");
	for (i = 0; i < *shmNumGxacts; i++)
	{
		elog(LOG, "%d - GID: %s  STATE: %s",
			 i, shmGxactArray[i]->gid,
			 DtxStateToString(shmGxactArray[i]->state));
	}
}


static void
dumpRMOnlyDtx(HTAB *htab, StringInfoData *buff)
{
	HASH_SEQ_STATUS status;
	InDoubtDtx *entry = NULL;

	if (htab == NULL)
		return;

	hash_seq_init(&status, htab);

	appendStringInfo(buff, "List of In-doubt transactions remaining across the segdbs: (");

	while ((entry = (InDoubtDtx *) hash_seq_search(&status)) != NULL)
	{
		appendStringInfo(buff, "\"%s\" , ", entry->gid);
	}

	appendStringInfo(buff, ")");

}


/*
 * When called, a SET command is dispatched and the writer gang
 * writes the shared snapshot. This function actually does nothing
 * useful besides making sure that a writer gang is alive and has
 * set the shared snapshot so that the readers could access it.
 *
 * At this point this function is added as a helper for cursor
 * query execution since in MPP cursor queries don't use writer
 * gangs. However, it could be used for other purposes as well.
 *
 * See declaration of assign_gp_write_shared_snapshot(...) for more
 * information.
 */
void
verify_shared_snapshot_ready(void)
{
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbDispatchCommand("set gp_write_shared_snapshot=true",
						   DF_CANCEL_ON_ERROR |
						   DF_WITH_SNAPSHOT |
						   DF_NEED_TWO_PHASE,
						   NULL);

		dumpSharedLocalSnapshot_forCursor();

		/*
		 * To keep our readers in sync (since they will be dispatched
		 * separately) we need to rewind the segmate synchronizer.
		 */
		DtxContextInfo_RewindSegmateSync();
	}
}

/*
 * Force the writer QE to write the shared snapshot. Will get called
 * after a "set gp_write_shared_snapshot=<true/false>" is executed
 * in dispatch mode.
 *
 * See verify_shared_snapshot_ready(...) for additional information.
 */
bool
assign_gp_write_shared_snapshot(bool newval, bool doit, GucSource source __attribute__((unused)))
{

#if FALSE
	elog(DEBUG1, "SET gp_write_shared_snapshot: %s, doit=%s",
		 (newval ? "true" : "false"), (doit ? "true" : "false"));
#endif

	/*
	 * Make sure newval is "true". if it's "false" this could be a part of a
	 * ROLLBACK so we don't want to set the snapshot then.
	 */
	if (doit && newval)
	{
		if (Gp_role == GP_ROLE_EXECUTE)
		{
			ActiveSnapshot = CopySnapshot(GetTransactionSnapshot());

			if (Gp_is_writer)
			{
				dumpSharedLocalSnapshot_forCursor();
			}
		}
	}

	return true;
}

static void
doQEDistributedExplicitBegin(int txnOptions)
{
	int			ExplicitIsoLevel;
	bool		ExplicitReadOnly;

	/*
	 * Start a command.
	 */
	StartTransactionCommand();

	/* Here is the explicit BEGIN. */
	BeginTransactionBlock();

	unpackMppTxnOptions(txnOptions,
						&ExplicitIsoLevel, &ExplicitReadOnly);

	XactIsoLevel = ExplicitIsoLevel;
	XactReadOnly = ExplicitReadOnly;

	elog(DTM_DEBUG5, "doQEDistributedExplicitBegin setting XactIsoLevel = %s and XactReadOnly = %s",
		 IsoLevelAsUpperString(XactIsoLevel), (XactReadOnly ? "true" : "false"));

	/*
	 * Finish the BEGIN command.  It will leave the explict transaction
	 * in-progress.
	 */
	CommitTransactionCommand();

}

static bool
isDtxQueryDispatcher(void)
{
	bool		isDtmStarted;
	bool		isSharedLocalSnapshotSlotPresent;

	isDtmStarted = (shmDtmStarted != NULL && *shmDtmStarted);
	isSharedLocalSnapshotSlotPresent = (SharedLocalSnapshotSlot != NULL);

	return (Gp_role == GP_ROLE_DISPATCH &&
			isDtmStarted &&
			isSharedLocalSnapshotSlotPresent);
}

/*
 * Called prior to handling a requested that comes to the QD, or a utility request to a QE.
 *
 * Sets up the distributed transaction context value and does some basic error checking.
 *
 * Essentially:
 *     if the DistributedTransactionContext is already QD_DISTRIBUTED_CAPABLE then leave it
 *     else if the DistributedTransactionContext is already QE_TWO_PHASE_EXPLICIT_WRITER then leave it
 *     else it MUST be a LOCAL_ONLY, and is converted to QD_DISTRIBUTED_CAPABLE if this process is acting
 *          as a QE.
 */
void
setupRegularDtxContext(void)
{
	switch (DistributedTransactionContext)
	{
		case DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE:
			/* Continue in this context.  Do not touch QEDtxContextInfo, etc. */
			break;

		case DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER:
			/* Allow this for copy...???  Do not touch QEDtxContextInfo, etc. */
			break;

		default:
			if (DistributedTransactionContext != DTX_CONTEXT_LOCAL_ONLY)
			{
				/*
				 * we must be one of:
				 *
				 * DTX_CONTEXT_QD_RETRY_PHASE_2,
				 * DTX_CONTEXT_QE_ENTRY_DB_SINGLETON,
				 * DTX_CONTEXT_QE_AUTO_COMMIT_IMPLICIT,
				 * DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER,
				 * DTX_CONTEXT_QE_READER, DTX_CONTEXT_QE_PREPARED
				 */

				elog(ERROR, "setupRegularDtxContext finds unexpected DistributedTransactionContext = '%s'",
					 DtxContextToString(DistributedTransactionContext));
			}

			/* DistributedTransactionContext is DTX_CONTEXT_LOCAL_ONLY */

			Assert(QEDtxContextInfo.distributedXid == InvalidDistributedTransactionId);

			/*
			 * Determine if we are strictly local or a distributed capable QD.
			 */
			Assert(DistributedTransactionContext == DTX_CONTEXT_LOCAL_ONLY);

			if (isDtxQueryDispatcher())
			{
				setDistributedTransactionContext(DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE);
			}
			break;
	}

	elog(DTM_DEBUG5, "setupRegularDtxContext leaving with DistributedTransactionContext = '%s'.",
		 DtxContextToString(DistributedTransactionContext));
}

/**
 * Called on the QE when a query to process has been received.
 *
 * This will set up all distributed transaction information and set the state appropriately.
 */
void
setupQEDtxContext(DtxContextInfo *dtxContextInfo)
{
	DistributedSnapshot *distributedSnapshot;
	int			txnOptions;
	bool		needTwoPhase;
	bool		explicitBegin;
	bool		haveDistributedSnapshot;
	bool		isEntryDbSingleton = false;
	bool		isReaderQE = false;
	bool		isWriterQE = false;
	bool		isSharedLocalSnapshotSlotPresent;

	Assert(dtxContextInfo != NULL);

	/*
	 * DTX Context Info (even when empty) only comes in QE requests.
	 */
	distributedSnapshot = &dtxContextInfo->distributedSnapshot;
	txnOptions = dtxContextInfo->distributedTxnOptions;

	needTwoPhase = isMppTxOptions_NeedTwoPhase(txnOptions);
	explicitBegin = isMppTxOptions_ExplicitBegin(txnOptions);

	haveDistributedSnapshot =
		(dtxContextInfo->distributedXid != InvalidDistributedTransactionId);
	isSharedLocalSnapshotSlotPresent = (SharedLocalSnapshotSlot != NULL);

	if (DEBUG5 >= log_min_messages || Debug_print_full_dtm)
	{
		elog(DTM_DEBUG5,
			 "setupQEDtxContext inputs (part 1): Gp_role = %s, Gp_is_writer = %s, "
			 "txnOptions = 0x%x, needTwoPhase = %s, explicitBegin = %s, isoLevel = %s, readOnly = %s, haveDistributedSnapshot = %s.",
			 role_to_string(Gp_role), (Gp_is_writer ? "true" : "false"), txnOptions,
			 (needTwoPhase ? "true" : "false"), (explicitBegin ? "true" : "false"),
			 IsoLevelAsUpperString(mppTxOptions_IsoLevel(txnOptions)), (isMppTxOptions_ReadOnly(txnOptions) ? "true" : "false"),
			 (haveDistributedSnapshot ? "true" : "false"));
		elog(DTM_DEBUG5,
			 "setupQEDtxContext inputs (part 2): distributedXid = %u, isSharedLocalSnapshotSlotPresent = %s.",
			 dtxContextInfo->distributedXid,
			 (isSharedLocalSnapshotSlotPresent ? "true" : "false"));

		if (haveDistributedSnapshot)
		{
			elog(DTM_DEBUG5,
				 "setupQEDtxContext inputs (part 2a): distributedXid = %u, "
				 "distributedSnapshotData (xmin = %u, xmax = %u, xcnt = %u), distributedCommandId = %d",
				 dtxContextInfo->distributedXid,
				 distributedSnapshot->xmin, distributedSnapshot->xmax,
				 distributedSnapshot->count,
				 dtxContextInfo->curcid);
		}
		if (isSharedLocalSnapshotSlotPresent)
		{
			if (DTM_DEBUG5 >= log_min_messages)
			{
				LWLockAcquire(SharedLocalSnapshotSlot->slotLock, LW_SHARED);
				elog(DTM_DEBUG5,
					 "setupQEDtxContext inputs (part 2b):  shared local snapshot xid = %u "
					 "(xmin: %u xmax: %u xcnt: %u) curcid: %d, QDxid = %u/%u, QDcid = %u",
					 SharedLocalSnapshotSlot->xid,
					 SharedLocalSnapshotSlot->snapshot.xmin,
					 SharedLocalSnapshotSlot->snapshot.xmax,
					 SharedLocalSnapshotSlot->snapshot.xcnt,
					 SharedLocalSnapshotSlot->snapshot.curcid,
					 SharedLocalSnapshotSlot->QDxid,
					 SharedLocalSnapshotSlot->segmateSync,
					 SharedLocalSnapshotSlot->QDcid);
				LWLockRelease(SharedLocalSnapshotSlot->slotLock);
			}
		}
	}

	switch (Gp_role)
	{
		case GP_ROLE_EXECUTE:
			if (Gp_segment == -1 && !Gp_is_writer)
			{
				isEntryDbSingleton = true;
			}
			else
			{
				/*
				 * NOTE: this is a bit hackish. It appears as though
				 * StartTransaction() gets called during connection setup
				 * before we even have time to setup our shared snapshot slot.
				 */
				if (SharedLocalSnapshotSlot == NULL)
				{
					if (explicitBegin || haveDistributedSnapshot)
					{
						elog(ERROR, "setupQEDtxContext not expecting distributed begin or snapshot when no Snapshot slot exists");
					}
				}
				else
				{
					if (Gp_is_writer)
					{
						isWriterQE = true;
					}
					else
					{
						isReaderQE = true;
					}
				}
			}
			break;

		default:
			Assert(DistributedTransactionContext == DTX_CONTEXT_LOCAL_ONLY);
			elog(DTM_DEBUG5,
				 "setupQEDtxContext leaving context = 'Local Only' for Gp_role = %s", role_to_string(Gp_role));
			return;
	}

	elog(DTM_DEBUG5,
		 "setupQEDtxContext intermediate result: isEntryDbSingleton = %s, isWriterQE = %s, isReaderQE = %s.",
		 (isEntryDbSingleton ? "true" : "false"),
		 (isWriterQE ? "true" : "false"), (isReaderQE ? "true" : "false"));

	/*
	 * Copy to our QE global variable.
	 */
	DtxContextInfo_Copy(&QEDtxContextInfo, dtxContextInfo);

	switch (DistributedTransactionContext)
	{
		case DTX_CONTEXT_LOCAL_ONLY:
			if (isEntryDbSingleton && haveDistributedSnapshot)
			{
				/*
				 * Later, in GetSnapshotData, we will adopt the QD's
				 * transaction and snapshot information.
				 */

				setDistributedTransactionContext(DTX_CONTEXT_QE_ENTRY_DB_SINGLETON);
			}
			else if (isReaderQE && haveDistributedSnapshot)
			{
				/*
				 * Later, in GetSnapshotData, we will adopt the QE Writer's
				 * transaction and snapshot information.
				 */

				setDistributedTransactionContext(DTX_CONTEXT_QE_READER);
			}
			else if (isWriterQE && (explicitBegin || needTwoPhase))
			{
				if (!haveDistributedSnapshot)
				{
					elog(DTM_DEBUG5,
						 "setupQEDtxContext Segment Writer is involved in a distributed transaction without a distributed snapshot...");
				}

				if (IsTransactionOrTransactionBlock())
				{
					elog(ERROR, "Starting an explicit distributed transaction in segment -- cannot already be in a transaction");
				}

				if (explicitBegin)
				{
					/*
					 * We set the DistributedTransactionContext BEFORE we
					 * create the transactions to influence the behavior of
					 * StartTransaction.
					 */
					setDistributedTransactionContext(DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER);

					doQEDistributedExplicitBegin(txnOptions);
				}
				else
				{
					Assert(needTwoPhase);
					setDistributedTransactionContext(DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER);
				}
			}
			else if (haveDistributedSnapshot)
			{
				if (IsTransactionOrTransactionBlock())
				{
					elog(ERROR,
						 "Going to start a local implicit transaction in segment using a distribute "
						 "snapshot -- cannot already be in a transaction");
				}

				/*
				 * Before executing the query, postgres.c make a standard call
				 * to StartTransactionCommand which will begin a local
				 * transaction with StartTransaction.  This is fine.
				 *
				 * However, when the snapshot is created later, the state
				 * below will tell GetSnapshotData to make the local snapshot
				 * from the distributed snapshot.
				 */
				setDistributedTransactionContext(DTX_CONTEXT_QE_AUTO_COMMIT_IMPLICIT);
			}
			else
			{
				Assert(!haveDistributedSnapshot);

				/*
				 * A local implicit transaction without reference to a
				 * distributed snapshot.  Stay in NONE state.
				 */
				Assert(DistributedTransactionContext == DTX_CONTEXT_LOCAL_ONLY);
			}
			break;

		case DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER:
/*
		elog(NOTICE, "We should have left this transition state '%s' at the end of the previous command...",
			 DtxContextToString(DistributedTransactionContext));
*/
			Assert(IsTransactionOrTransactionBlock());

			if (explicitBegin)
			{
				elog(ERROR, "Cannot have an explicit BEGIN statement...");
			}
			break;

		case DTX_CONTEXT_QE_AUTO_COMMIT_IMPLICIT:
			elog(ERROR, "We should have left this transition state '%s' at the end of the previous command",
				 DtxContextToString(DistributedTransactionContext));
			break;

		case DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER:
			Assert(IsTransactionOrTransactionBlock());
			break;

		case DTX_CONTEXT_QE_ENTRY_DB_SINGLETON:
		case DTX_CONTEXT_QE_READER:

			/*
			 * We are playing games with the xact.c code, so we shouldn't test
			 * with the IsTransactionOrTransactionBlock() routine.
			 */
			break;

		case DTX_CONTEXT_QE_PREPARED:
		case DTX_CONTEXT_QE_FINISH_PREPARED:
			elog(ERROR, "We should not be trying to execute a query in state '%s'",
				 DtxContextToString(DistributedTransactionContext));
			break;

		default:
			elog(PANIC, "Unexpected segment distribute transaction context value: %d",
				 (int) DistributedTransactionContext);
			break;
	}

	elog(DTM_DEBUG5, "setupQEDtxContext final result: DistributedTransactionContext = '%s'.",
		 DtxContextToString(DistributedTransactionContext));

	if (haveDistributedSnapshot)
	{
		elog((Debug_print_snapshot_dtm ? LOG : DEBUG5), "[Distributed Snapshot #%u] *Set QE* currcid = %d (gxid = %u, '%s')",
			 dtxContextInfo->distributedSnapshot.distribSnapshotId,
			 dtxContextInfo->curcid,
			 getDistributedTransactionId(),
			 DtxContextToString(DistributedTransactionContext));
	}

}

void
finishDistributedTransactionContext(char *debugCaller, bool aborted)
{
	DistributedTransactionId gxid;

	/*
	 * We let the 2 retry states go up to PostgresMain.c, otherwise everything
	 * MUST be complete.
	 */
	if (currentGxact != NULL &&
		(currentGxact->state != DTX_STATE_RETRY_COMMIT_PREPARED &&
		 currentGxact->state != DTX_STATE_RETRY_ABORT_PREPARED))
	{
		elog(FATAL, "Expected currentGxact to be NULL at this point.  Found gid =%s, gxid = %u (state = %s, caller = %s)",
			 currentGxact->gid, currentGxact->gxid, DtxStateToString(currentGxact->state), debugCaller);
	}

	gxid = getDistributedTransactionId();
	elog(DTM_DEBUG5,
		 "finishDistributedTransactionContext called to change DistributedTransactionContext from %s to %s (caller = %s, gxid = %u)",
		 DtxContextToString(DistributedTransactionContext),
		 DtxContextToString(DTX_CONTEXT_LOCAL_ONLY),
		 debugCaller,
		 gxid);

	setDistributedTransactionContext(DTX_CONTEXT_LOCAL_ONLY);

	DtxContextInfo_Reset(&QEDtxContextInfo);

}

static void
rememberDtxExplicitBegin(void)
{
	if (currentGxact == NULL)
	{
		return;
	}

	if (!currentGxact->explicitBeginRemembered)
	{
		elog(DTM_DEBUG5, "rememberDtxExplicitBegin explicit BEGIN for gid = %s",
			 currentGxact->gid);
		currentGxact->explicitBeginRemembered = true;
	}
	else
	{
		elog(DTM_DEBUG5, "rememberDtxExplicitBegin already an explicit BEGIN for gid = %s",
			 currentGxact->gid);
	}
}

/*
 * This is mostly here because
 * cdbcopy doesn't use cdbdisp's services.
 */
void
sendDtxExplicitBegin(void)
{
	char		cmdbuf[100];

	if (currentGxact == NULL)
	{
		return;
	}

	rememberDtxExplicitBegin();

	dtmPreCommand("sendDtxExplicitBegin", "(none)", NULL,
				   /* is two-phase */ true, /* withSnapshot */ true, /* inCursor */ false);

	/*
	 * Be explicit about both the isolation level and the access mode since in
	 * MPP our QEs are in a another process.
	 */
	sprintf(cmdbuf, "BEGIN ISOLATION LEVEL %s, READ %s",
			IsoLevelAsUpperString(XactIsoLevel),
			(XactReadOnly ? "ONLY" : "WRITE"));

	/*
	 * dispatch a DTX command, in the event of an error, this call will either
	 * exit via elog()/ereport() or return false
	 */
	if (!dispatchDtxCommand(cmdbuf))
	{
		ereport(ERROR, (errmsg("Global transaction BEGIN failed for gid = \"%s\" due to error",
							   currentGxact->gid)));
	}
}

/**
 * On the QD, run the Prepare operation.
 */
static void
performDtxProtocolPrepare(const char *gid)
{
	StartTransactionCommand();

	elog(DTM_DEBUG5, "performDtxProtocolCommand going to call PrepareTransactionBlock for distributed transaction (id = '%s')", gid);
	if (!PrepareTransactionBlock((char *) gid))
	{
		elog(ERROR, "Prepare of distributed transaction %s failed", gid);
		return;
	}

	/*
	 * Calling CommitTransactionCommand will cause the actual COMMIT/PREPARE
	 * work to be performed.
	 */
	CommitTransactionCommand();

	elog(DTM_DEBUG5, "Prepare of distributed transaction succeeded (id = '%s')", gid);

	setDistributedTransactionContext(DTX_CONTEXT_QE_PREPARED);
}

/**
 * On the QD, run the Commit Prepared operation.
 */
static void
performDtxProtocolCommitPrepared(const char *gid, bool raiseErrorIfNotFound)
{
	elog(DTM_DEBUG5,
		 "performDtxProtocolCommitPrepared going to call FinishPreparedTransaction for distributed transaction %s", gid);

	StartTransactionCommand();

	/*
	 * Since this call may fail, lets setup a handler.
	 */
	PG_TRY();
	{
		FinishPreparedTransaction((char *) gid, /* isCommit */ true, raiseErrorIfNotFound);
	}
	PG_CATCH();
	{
		finishDistributedTransactionContext("performDtxProtocolCommitPrepared -- Commit Prepared (error case)", false);
		PG_RE_THROW();
	}
	PG_END_TRY();

	/*
	 * Calling CommitTransactionCommand will cause the actual COMMIT/PREPARE
	 * work to be performed.
	 */
	CommitTransactionCommand();

	finishDistributedTransactionContext("performDtxProtocolCommitPrepared -- Commit Prepared", false);
}

/**
 * On the QD, run the Abort Prepared operation.
 */
static void
performDtxProtocolAbortPrepared(const char *gid, bool raiseErrorIfNotFound)
{
	elog(DTM_DEBUG5, "performDtxProtocolAbortPrepared going to call FinishPreparedTransaction for distributed transaction %s", gid);

	StartTransactionCommand();

	/*
	 * Since this call may fail, lets setup a handler.
	 */
	PG_TRY();
	{
		FinishPreparedTransaction((char *) gid, /* isCommit */ false, raiseErrorIfNotFound);
	}
	PG_CATCH();
	{
		finishDistributedTransactionContext("performDtxProtocolAbortPrepared -- Commit Prepared (error case)", true);
		PG_RE_THROW();
	}
	PG_END_TRY();

	/*
	 * Calling CommitTransactionCommand will cause the actual COMMIT/PREPARE
	 * work to be performed.
	 */
	CommitTransactionCommand();

	finishDistributedTransactionContext("performDtxProtocolAbortPrepared -- Commit Prepared", true);
}

/**
 * On the QE, handle a DtxProtocolCommand
 */
void
performDtxProtocolCommand(DtxProtocolCommand dtxProtocolCommand,
						  int flags __attribute__((unused)),
						  const char *loggingStr __attribute__((unused)), const char *gid,
						  DistributedTransactionId gxid __attribute__((unused)),
						  DtxContextInfo *contextInfo)
{
	elog(DTM_DEBUG5,
		 "performDtxProtocolCommand called with DTX protocol = %s, segment distribute transaction context: '%s'",
		 DtxProtocolCommandToString(dtxProtocolCommand), DtxContextToString(DistributedTransactionContext));

	switch (dtxProtocolCommand)
	{
		case DTX_PROTOCOL_COMMAND_STAY_AT_OR_BECOME_IMPLIED_WRITER:
			switch (DistributedTransactionContext)
			{
				case DTX_CONTEXT_LOCAL_ONLY:
					/** convert to implicit_writer! */
					setupQEDtxContext(contextInfo);
					StartTransactionCommand();
					break;
				case DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER:
					/** already the state we like */
					break;
				default:
					if (isQEContext() || isQDContext())
					{
						elog(FATAL, "Unexpected segment distributed transaction context: '%s'",
							 DtxContextToString(DistributedTransactionContext));
					}
					else
					{
						elog(PANIC, "Unexpected segment distributed transaction context value: %d",
							 (int) DistributedTransactionContext);
					}
					break;
			}
			break;

		case DTX_PROTOCOL_COMMAND_ABORT_NO_PREPARED:
			elog(DTM_DEBUG5,
				 "performDtxProtocolCommand going to call AbortOutOfAnyTransaction for distributed transaction %s", gid);
			AbortOutOfAnyTransaction();
			break;

		case DTX_PROTOCOL_COMMAND_PREPARE:

			/*
			 * The QD has directed us to read-only commit or prepare an
			 * implicit or explicit distributed transaction.
			 */
			switch (DistributedTransactionContext)
			{
				case DTX_CONTEXT_LOCAL_ONLY:

					/*
					 * Spontaneously aborted while we were back at the QD?
					 */
					elog(ERROR, "Distributed transaction %s not found", gid);
					break;

				case DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER:
				case DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER:
					performDtxProtocolPrepare(gid);
					break;

				case DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE:
				case DTX_CONTEXT_QD_RETRY_PHASE_2:
				case DTX_CONTEXT_QE_PREPARED:
				case DTX_CONTEXT_QE_FINISH_PREPARED:
				case DTX_CONTEXT_QE_ENTRY_DB_SINGLETON:
				case DTX_CONTEXT_QE_READER:
					elog(FATAL, "Unexpected segment distribute transaction context: '%s'",
						 DtxContextToString(DistributedTransactionContext));

				default:
					elog(PANIC, "Unexpected segment distribute transaction context value: %d",
						 (int) DistributedTransactionContext);
					break;
			}
			break;

		case DTX_PROTOCOL_COMMAND_ABORT_SOME_PREPARED:
			switch (DistributedTransactionContext)
			{
				case DTX_CONTEXT_LOCAL_ONLY:

					/*
					 * Spontaneously aborted while we were back at the QD?
					 */
					elog(ERROR, "Distributed transaction %s not found", gid);
					break;

				case DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER:
				case DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER:
					AbortOutOfAnyTransaction();
					break;

				case DTX_CONTEXT_QE_PREPARED:
					setDistributedTransactionContext(DTX_CONTEXT_QE_FINISH_PREPARED);
					performDtxProtocolAbortPrepared(gid, /* raiseErrorIfNotFound */ true);
					break;

				case DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE:
				case DTX_CONTEXT_QD_RETRY_PHASE_2:
				case DTX_CONTEXT_QE_ENTRY_DB_SINGLETON:
				case DTX_CONTEXT_QE_READER:
					elog(PANIC, "Unexpected segment distribute transaction context: '%s'",
						 DtxContextToString(DistributedTransactionContext));

				default:
					elog(PANIC, "Unexpected segment distribute transaction context value: %d",
						 (int) DistributedTransactionContext);
					break;
			}
			break;

		case DTX_PROTOCOL_COMMAND_COMMIT_PREPARED:
			requireDistributedTransactionContext(DTX_CONTEXT_QE_PREPARED);
			setDistributedTransactionContext(DTX_CONTEXT_QE_FINISH_PREPARED);
			performDtxProtocolCommitPrepared(gid, /* raiseErrorIfNotFound */ true);
			break;

		case DTX_PROTOCOL_COMMAND_ABORT_PREPARED:
			requireDistributedTransactionContext(DTX_CONTEXT_QE_PREPARED);
			setDistributedTransactionContext(DTX_CONTEXT_QE_FINISH_PREPARED);
			performDtxProtocolAbortPrepared(gid, /* raiseErrorIfNotFound */ true);
			break;

		case DTX_PROTOCOL_COMMAND_RETRY_COMMIT_PREPARED:
			requireDistributedTransactionContext(DTX_CONTEXT_LOCAL_ONLY);
			performDtxProtocolCommitPrepared(gid, /* raiseErrorIfNotFound */ false);
			break;

		case DTX_PROTOCOL_COMMAND_RETRY_ABORT_PREPARED:
			requireDistributedTransactionContext(DTX_CONTEXT_LOCAL_ONLY);
			performDtxProtocolAbortPrepared(gid, /* raiseErrorIfNotFound */ false);
			break;

		case DTX_PROTOCOL_COMMAND_RECOVERY_COMMIT_PREPARED:
			requireDistributedTransactionContext(DTX_CONTEXT_LOCAL_ONLY);
			performDtxProtocolCommitPrepared(gid, /* raiseErrorIfNotFound */ false);
			break;

		case DTX_PROTOCOL_COMMAND_RECOVERY_ABORT_PREPARED:
			requireDistributedTransactionContext(DTX_CONTEXT_LOCAL_ONLY);
			performDtxProtocolAbortPrepared(gid, /* raiseErrorIfNotFound */ false);
			break;

		case DTX_PROTOCOL_COMMAND_SUBTRANSACTION_BEGIN_INTERNAL:
			switch (DistributedTransactionContext)
			{
				case DTX_CONTEXT_LOCAL_ONLY:

					/*
					 * QE is not aware of DTX yet. A typical case is SELECT
					 * foo(), where foo() opens internal subtransaction
					 */
					setupQEDtxContext(contextInfo);
					StartTransactionCommand();
					break;
				case DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER:

					/*
					 * We already marked this QE to be writer, and transaction
					 * is open.
					 */
				case DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER:
				case DTX_CONTEXT_QE_READER:
					break;
				default:
					/* Lets flag this situation out, with explicit crash */
					Assert(false);
					elog(DTM_DEBUG5,
						 " SUBTRANSACTION_BEGIN_INTERNAL distributed transaction context invalid: %d",
						 (int) DistributedTransactionContext);
					break;
			}

			BeginInternalSubTransaction(NULL);
			Assert(contextInfo->nestingLevel + 1 == GetCurrentTransactionNestLevel());
			break;

		case DTX_PROTOCOL_COMMAND_SUBTRANSACTION_RELEASE_INTERNAL:
			Assert(contextInfo->nestingLevel == GetCurrentTransactionNestLevel());
			ReleaseCurrentSubTransaction();
			break;

		case DTX_PROTOCOL_COMMAND_SUBTRANSACTION_ROLLBACK_INTERNAL:

			/*
			 * Rollback performs work on master and then dispatches, hence has
			 * nestingLevel its expecting post operation
			 */
			if ((contextInfo->nestingLevel + 1) > GetCurrentTransactionNestLevel())
			{
				ereport(ERROR,
						(errmsg("transaction %s at level %d already processed (current level %d)",
								gid, contextInfo->nestingLevel, GetCurrentTransactionNestLevel())));
			}

			unsigned int i = GetCurrentTransactionNestLevel() - contextInfo->nestingLevel;

			while (i > 0)
			{
				RollbackAndReleaseCurrentSubTransaction();
				i--;
			}

			Assert(contextInfo->nestingLevel == GetCurrentTransactionNestLevel());
			break;

		default:
			elog(ERROR, "Unrecognized dtx protocol command: %d",
				 (int) dtxProtocolCommand);
			break;
	}
	elog(DTM_DEBUG5, "performDtxProtocolCommand successful return for distributed transaction %s", gid);
}
