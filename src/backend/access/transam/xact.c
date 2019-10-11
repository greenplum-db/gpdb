/*-------------------------------------------------------------------------
 *
 * xact.c
 *	  top level transaction system support routines
 *
 * See src/backend/access/transam/README for more information.
 *
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/transam/xact.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <time.h>
#include <unistd.h>

#include "access/appendonlywriter.h"
#include "access/multixact.h"
#include "access/subtrans.h"
#include "access/transam.h"
#include "access/twophase.h"
#include "access/xact.h"
#include "access/xact_storage_tablespace.h"
#include "access/xlogutils.h"
#include "catalog/catalog.h"
#include "catalog/namespace.h"
#include "catalog/oid_dispatch.h"
#include "catalog/storage.h"
#include "catalog/storage_tablespace.h"
#include "catalog/storage_database.h"
#include "commands/async.h"
#include "commands/dbcommands.h"
#include "commands/resgroupcmds.h"
#include "commands/tablecmds.h"
#include "commands/trigger.h"
#include "executor/spi.h"
#include "libpq/be-fsstubs.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "replication/walsender.h"
#include "replication/syncrep.h"
#include "storage/bufmgr.h"
#include "storage/fd.h"
#include "storage/freespace.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/sinvaladt.h"
#include "storage/smgr.h"
#include "utils/catcache.h"
#include "utils/combocid.h"
#include "utils/faultinjector.h"
#include "utils/guc.h"
#include "utils/inval.h"
#include "utils/memutils.h"
#include "utils/relmapper.h"

#include "utils/builtins.h"
#include "utils/resource_manager.h"
#include "utils/sharedsnapshot.h"
#include "utils/snapmgr.h"
#include "utils/timeout.h"
#include "utils/timestamp.h"
#include "pg_trace.h"

#include "access/distributedlog.h"
#include "cdb/cdbdistributedsnapshot.h"
#include "cdb/cdbgang.h"
#include "cdb/cdblocaldistribxact.h"
#include "cdb/cdbtm.h"
#include "cdb/cdbvars.h" /* Gp_role, Gp_is_writer, interconnect_setup_timeout */
#include "utils/vmem_tracker.h"
#include "cdb/cdbdisp.h"

/*
 *	User-tweakable parameters
 */
int			DefaultXactIsoLevel = XACT_READ_COMMITTED;
int			XactIsoLevel;

bool		DefaultXactReadOnly = false;
bool		XactReadOnly;

bool		DefaultXactDeferrable = false;
bool		XactDeferrable;

int			synchronous_commit = SYNCHRONOUS_COMMIT_ON;

/*
 * MyXactAccessedTempRel is set when a temporary relation is accessed.
 * We don't allow PREPARE TRANSACTION in that case.  (This is global
 * so that it can be set from heapam.c.)
 *
 * Not used in GPDB, see comments in PrepareTransaction()
 */
#if 0
bool		MyXactAccessedTempRel = false;
#endif

int32 gp_subtrans_warn_limit = 16777216; /* 16 million */

/* gp-specific
 * routine for marking when a sequence makes a mark in the xlog.  we need
 * to keep track of this because sequences are the only reason a reader should
 * ever write to the xlog during commit.  As a result, we keep track of such
 * and will complain loudly if its violated.
 */
bool		seqXlogWrite;

/*
 *	transaction states - transaction state from server perspective
 */
typedef enum TransState
{
	TRANS_DEFAULT,				/* idle */
	TRANS_START,				/* transaction starting */
	TRANS_INPROGRESS,			/* inside a valid transaction */
	TRANS_COMMIT,				/* commit in progress */
	TRANS_ABORT,				/* abort in progress */
	TRANS_PREPARE				/* prepare in progress */
} TransState;

/*
 *	transaction block states - transaction state of client queries
 *
 * Note: the subtransaction states are used only for non-topmost
 * transactions; the others appear only in the topmost transaction.
 */
typedef enum TBlockState
{
	/* not-in-transaction-block states */
	TBLOCK_DEFAULT,				/* idle */
	TBLOCK_STARTED,				/* running single-query transaction */

	/* transaction block states */
	TBLOCK_BEGIN,				/* starting transaction block */
	TBLOCK_INPROGRESS,			/* live transaction */
	TBLOCK_END,					/* COMMIT received */
	TBLOCK_ABORT,				/* failed xact, awaiting ROLLBACK */
	TBLOCK_ABORT_END,			/* failed xact, ROLLBACK received */
	TBLOCK_ABORT_PENDING,		/* live xact, ROLLBACK received */
	TBLOCK_PREPARE,				/* live xact, PREPARE received */

	/* subtransaction states */
	TBLOCK_SUBBEGIN,			/* starting a subtransaction */
	TBLOCK_SUBINPROGRESS,		/* live subtransaction */
	TBLOCK_SUBRELEASE,			/* RELEASE received */
	TBLOCK_SUBCOMMIT,			/* COMMIT received while TBLOCK_SUBINPROGRESS */
	TBLOCK_SUBABORT,			/* failed subxact, awaiting ROLLBACK */
	TBLOCK_SUBABORT_END,		/* failed subxact, ROLLBACK received */
	TBLOCK_SUBABORT_PENDING,	/* live subxact, ROLLBACK received */
	TBLOCK_SUBRESTART,			/* live subxact, ROLLBACK TO received */
	TBLOCK_SUBABORT_RESTART		/* failed subxact, ROLLBACK TO received */
} TBlockState;

/*
 *	transaction state structure
 */
typedef struct TransactionStateData
{
	TransactionId transactionId;	/* my XID, or Invalid if none */
	SubTransactionId subTransactionId;	/* my subxact ID */
	char	   *name;			/* savepoint name, if any */
	int			savepointLevel; /* savepoint level */
	TransState	state;			/* low-level state */
	TBlockState blockState;		/* high-level state */
	int			nestingLevel;	/* transaction nesting depth */
	int			gucNestLevel;	/* GUC context nesting depth */
	MemoryContext curTransactionContext;		/* my xact-lifetime context */
	ResourceOwner curTransactionOwner;	/* my query resources */
	TransactionId *childXids;	/* subcommitted child XIDs, in XID order */
	int			nChildXids;		/* # of subcommitted child XIDs */
	int			maxChildXids;	/* allocated size of childXids[] */
	Oid			prevUser;		/* previous CurrentUserId setting */
	int			prevSecContext; /* previous SecurityRestrictionContext */
	bool		prevXactReadOnly;		/* entry-time xact r/o state */
	bool		startedInRecovery;		/* did we start in recovery? */
	bool		didLogXid;		/* has xid been included in WAL record? */
	bool		executorSaysXactDoesWrites;	/* GP executor says xact does writes */
	bool		executorDidWriteXLog;	/* QE has wrote xlog */
	struct TransactionStateData *parent;		/* back link to parent */

	struct TransactionStateData *fastLink;        /* back link to jump to parent for efficient search */
} TransactionStateData;

typedef TransactionStateData *TransactionState;

#define NUM_NODES_TO_SKIP_FOR_FAST_SEARCH 100
static int fastNodeCount;
static TransactionState previousFastLink;

/*
 * CurrentTransactionState always points to the current transaction state
 * block.  It will point to TopTransactionStateData when not in a
 * transaction at all, or when in a top-level transaction.
 */
static TransactionStateData TopTransactionStateData = {
	0,							/* transaction id */
	0,							/* subtransaction id */
	NULL,						/* savepoint name */
	0,							/* savepoint level */
	TRANS_DEFAULT,				/* transaction state */
	TBLOCK_DEFAULT,				/* transaction block state from the client
								 * perspective */
	0,							/* transaction nesting depth */
	0,							/* GUC context nesting depth */
	NULL,						/* cur transaction context */
	NULL,						/* cur transaction resource owner */
	NULL,						/* subcommitted child Xids */
	0,							/* # of subcommitted child Xids */
	0,							/* allocated size of childXids[] */
	InvalidOid,					/* previous CurrentUserId setting */
	0,							/* previous SecurityRestrictionContext */
	false,						/* entry-time xact r/o state */
	false,						/* startedInRecovery */
	false,						/* didLogXid */
	false,						/* executorSaysXactDoesWrites */
	false,						/* executorDidWriteXLog */
	NULL						/* link to parent state block */
};

/*
 * unreportedXids holds XIDs of all subtransactions that have not yet been
 * reported in a XLOG_XACT_ASSIGNMENT record.
 */
static int	nUnreportedXids;
static TransactionId unreportedXids[PGPROC_MAX_CACHED_SUBXIDS];

static TransactionState CurrentTransactionState = &TopTransactionStateData;

/* distributed transaction id of current transaction, if any. */
static DistributedTransactionId currentDistribXid;

/*
 * The subtransaction ID and command ID assignment counters are global
 * to a whole transaction, so we do not keep them in the state stack.
 */
static SubTransactionId currentSubTransactionId;
static CommandId currentCommandId;
static bool currentCommandIdUsed;

/*
 * xactStartTimestamp is the value of transaction_timestamp().
 * stmtStartTimestamp is the value of statement_timestamp().
 * xactStopTimestamp is the time at which we log a commit or abort WAL record.
 * These do not change as we enter and exit subtransactions, so we don't
 * keep them inside the TransactionState stack.
 */
static TimestampTz xactStartTimestamp;
static TimestampTz stmtStartTimestamp;
static TimestampTz xactStopTimestamp;

/*
 * Total number of SAVEPOINT commands executed by this transaction.
 *
 */
static int currentSavepointTotal;

/*
 * GID to be used for preparing the current transaction.  This is also
 * global to a whole transaction, so we don't keep it in the state stack.
 */
static char *prepareGID;

/*
 * Some commands want to force synchronous commit.
 */
static bool forceSyncCommit = false;

/*
 * Private context for transaction-abort work --- we reserve space for this
 * at startup to ensure that AbortTransaction and AbortSubTransaction can work
 * when we've run out of memory.
 */
static MemoryContext TransactionAbortContext = NULL;

/*
 * List of add-on start- and end-of-xact callbacks
 */
typedef struct XactCallbackItem
{
	struct XactCallbackItem *next;
	XactCallback callback;
	void	   *arg;
} XactCallbackItem;

static XactCallbackItem *Xact_callbacks = NULL;
static XactCallbackItem *Xact_callbacks_once = NULL;

/*
 * List of add-on start- and end-of-subxact callbacks
 */
typedef struct SubXactCallbackItem
{
	struct SubXactCallbackItem *next;
	SubXactCallback callback;
	void	   *arg;
} SubXactCallbackItem;

static SubXactCallbackItem *SubXact_callbacks = NULL;

/*
 * Subtransaction file used to keep subtransaction Ids that spill over from
 * shared snapshot. Kept outside of shared snapshot because readers and writer
 * have their own File pointer.
 */
File subxip_file = 0;

/* local function prototypes */
static void AssignTransactionId(TransactionState s);
static void AbortTransaction(void);
static void AtAbort_Memory(void);
static void AtCleanup_Memory(void);
static void AtAbort_ResourceOwner(void);
static void AtCCI_LocalCache(void);
static void AtCommit_Memory(void);
static void AtStart_Cache(void);
static void AtStart_Memory(void);
static void AtStart_ResourceOwner(void);
static void CallXactCallbacks(XactEvent event);
static void CallXactCallbacksOnce(XactEvent event);
static void CallSubXactCallbacks(SubXactEvent event,
					 SubTransactionId mySubid,
					 SubTransactionId parentSubid);
static void CleanupTransaction(void);
static void CheckTransactionChain(bool isTopLevel, bool throwError,
					  const char *stmtType);
static void CommitTransaction(void);
static TransactionId RecordTransactionAbort(bool isSubXact);
static void StartTransaction(void);

static void StartSubTransaction(void);
static void CommitSubTransaction(void);
static void AbortSubTransaction(void);
static void CleanupSubTransaction(void);
static void PushTransaction(void);
static void PopTransaction(void);

static void AtSubAbort_Memory(void);
static void AtSubCleanup_Memory(void);
static void AtSubAbort_ResourceOwner(void);
static void AtSubCommit_Memory(void);
static void AtSubStart_Memory(void);
static void AtSubStart_ResourceOwner(void);

static void EndLocalDistribXact(bool isCommit);
static void ShowTransactionState(const char *str);
static void ShowTransactionStateRec(TransactionState state);
static const char *BlockStateAsString(TBlockState blockState);
static const char *TransStateAsString(TransState state);
static void DispatchRollbackToSavepoint(char *name);

static bool IsCurrentTransactionIdForReader(TransactionId xid);

/* ----------------------------------------------------------------
 *	transaction state accessors
 * ----------------------------------------------------------------
 */

/*
 *	IsTransactionState
 *
 *	This returns true if we are inside a valid transaction; that is,
 *	it is safe to initiate database access, take heavyweight locks, etc.
 */
bool
IsTransactionState(void)
{
	TransactionState s = CurrentTransactionState;

	/*
	 * TRANS_DEFAULT and TRANS_ABORT are obviously unsafe states.  However, we
	 * also reject the startup/shutdown states TRANS_START, TRANS_COMMIT,
	 * TRANS_PREPARE since it might be too soon or too late within those
	 * transition states to do anything interesting.  Hence, the only "valid"
	 * state is TRANS_INPROGRESS.
	 */
	return (s->state == TRANS_INPROGRESS);
}

bool
IsAbortInProgress(void)
{
	TransactionState s = CurrentTransactionState;

	return (s->state == TRANS_ABORT);
}

bool
IsTransactionPreparing(void)
{
	TransactionState s = CurrentTransactionState;

	return (s->state == TRANS_PREPARE);
}
/*
 *	IsAbortedTransactionBlockState
 *
 *	This returns true if we are within an aborted transaction block.
 */
bool
IsAbortedTransactionBlockState(void)
{
	TransactionState s = CurrentTransactionState;

	if (s->blockState == TBLOCK_ABORT ||
		s->blockState == TBLOCK_SUBABORT)
		return true;

	return false;
}

bool
TransactionDidWriteXLog(void)
{
	TransactionState s = CurrentTransactionState;
	return s->didLogXid;
}

bool
ExecutorDidWriteXLog(void)
{
	TransactionState s = CurrentTransactionState;
	return s->executorDidWriteXLog;
}

void
GetAllTransactionXids(
	DistributedTransactionId	*distribXid,
	TransactionId				*localXid,
	TransactionId				*subXid)
{
	TransactionState s = CurrentTransactionState;

	*distribXid = currentDistribXid;
	*localXid = s->transactionId;
	*subXid = s->subTransactionId;
}

DistributedTransactionId
GetCurrentDistributedTransactionId(void)
{
	return currentDistribXid;
}

void
SetCurrentDistributedTransactionId(DistributedTransactionId gxid)
{
	currentDistribXid = gxid;
}
/*
 *	GetTopTransactionId
 *
 * This will return the XID of the main transaction, assigning one if
 * it's not yet set.  Be careful to call this only inside a valid xact.
 */
TransactionId
GetTopTransactionId(void)
{
	if (!TransactionIdIsValid(TopTransactionStateData.transactionId))
		AssignTransactionId(&TopTransactionStateData);
	return TopTransactionStateData.transactionId;
}

/*
 *	GetTopTransactionIdIfAny
 *
 * This will return the XID of the main transaction, if one is assigned.
 * It will return InvalidTransactionId if we are not currently inside a
 * transaction, or inside a transaction that hasn't yet been assigned an XID.
 */
TransactionId
GetTopTransactionIdIfAny(void)
{
	return TopTransactionStateData.transactionId;
}

/*
 *	GetCurrentTransactionId
 *
 * This will return the XID of the current transaction (main or sub
 * transaction), assigning one if it's not yet set.  Be careful to call this
 * only inside a valid xact.
 */
TransactionId
GetCurrentTransactionId(void)
{
	TransactionState s = CurrentTransactionState;

	if (!TransactionIdIsValid(s->transactionId))
		AssignTransactionId(s);
	return s->transactionId;
}

/*
 *	GetCurrentTransactionIdIfAny
 *
 * This will return the XID of the current sub xact, if one is assigned.
 * It will return InvalidTransactionId if we are not currently inside a
 * transaction, or inside a transaction that hasn't been assigned an XID yet.
 */
TransactionId
GetCurrentTransactionIdIfAny(void)
{
	return CurrentTransactionState->transactionId;
}

/*
 *	MarkCurrentTransactionIdLoggedIfAny
 *
 * Remember that the current xid - if it is assigned - now has been wal logged.
 */
void
MarkCurrentTransactionIdLoggedIfAny(void)
{
	if (TransactionIdIsValid(CurrentTransactionState->transactionId))
		CurrentTransactionState->didLogXid = true;
}

void
MarkCurrentTransactionWriteXLogOnExecutor(void)
{
	CurrentTransactionState->executorDidWriteXLog = true;
}

/*
 *	GetStableLatestTransactionId
 *
 * Get the transaction's XID if it has one, else read the next-to-be-assigned
 * XID.  Once we have a value, return that same value for the remainder of the
 * current transaction.  This is meant to provide the reference point for the
 * age(xid) function, but might be useful for other maintenance tasks as well.
 */
TransactionId
GetStableLatestTransactionId(void)
{
	static LocalTransactionId lxid = InvalidLocalTransactionId;
	static TransactionId stablexid = InvalidTransactionId;

	if (lxid != MyProc->lxid)
	{
		lxid = MyProc->lxid;
		stablexid = GetTopTransactionIdIfAny();
		if (!TransactionIdIsValid(stablexid))
			stablexid = ReadNewTransactionId();
	}

	Assert(TransactionIdIsValid(stablexid));

	return stablexid;
}

/*
 * AssignTransactionId
 *
 * Assigns a new permanent XID to the given TransactionState.
 * We do not assign XIDs to transactions until/unless this is called.
 * Also, any parent TransactionStates that don't yet have XIDs are assigned
 * one; this maintains the invariant that a child transaction has an XID
 * following its parent's.
 */
static void
AssignTransactionId(TransactionState s)
{
	bool		isSubXact = (s->parent != NULL);
	ResourceOwner currentOwner;
	bool		log_unknown_top = false;

	/* Assert that caller didn't screw up */
	Assert(!TransactionIdIsValid(s->transactionId));
	Assert(s->state == TRANS_INPROGRESS);

	if (DistributedTransactionContext == DTX_CONTEXT_QE_READER ||
		DistributedTransactionContext == DTX_CONTEXT_QE_ENTRY_DB_SINGLETON)
	{
		elog(ERROR, "AssignTransactionId() called by %s process",
			 DtxContextToString(DistributedTransactionContext));
	}

	/*
	 * Ensure parent(s) have XIDs, so that a child always has an XID later
	 * than its parent.  Musn't recurse here, or we might get a stack overflow
	 * if we're at the bottom of a huge stack of subtransactions none of which
	 * have XIDs yet.
	 */
	if (isSubXact && !TransactionIdIsValid(s->parent->transactionId))
	{
		TransactionState p = s->parent;
		TransactionState *parents;
		size_t		parentOffset = 0;

		parents = palloc(sizeof(TransactionState) * s->nestingLevel);
		while (p != NULL && !TransactionIdIsValid(p->transactionId))
		{
			parents[parentOffset++] = p;
			p = p->parent;
		}

		/*
		 * This is technically a recursive call, but the recursion will never
		 * be more than one layer deep.
		 */
		while (parentOffset != 0)
			AssignTransactionId(parents[--parentOffset]);

		pfree(parents);
	}

	/*
	 * When wal_level=logical, guarantee that a subtransaction's xid can only
	 * be seen in the WAL stream if its toplevel xid has been logged before.
	 * If necessary we log a xact_assignment record with fewer than
	 * PGPROC_MAX_CACHED_SUBXIDS. Note that it is fine if didLogXid isn't set
	 * for a transaction even though it appears in a WAL record, we just might
	 * superfluously log something. That can happen when an xid is included
	 * somewhere inside a wal record, but not in XLogRecord->xl_xid, like in
	 * xl_standby_locks.
	 */
	if (isSubXact && XLogLogicalInfoActive() &&
		!TopTransactionStateData.didLogXid)
		log_unknown_top = true;

	/*
	 * Generate a new Xid and record it in PG_PROC and pg_subtrans.
	 *
	 * NB: we must make the subtrans entry BEFORE the Xid appears anywhere in
	 * shared storage other than PG_PROC; because if there's no room for it in
	 * PG_PROC, the subtrans entry is needed to ensure that other backends see
	 * the Xid as "running".  See GetNewTransactionId.
	 */
	s->transactionId = GetNewTransactionId(isSubXact);
	ereportif(Debug_print_full_dtm, LOG,
			  (errmsg("AssignTransactionId(): assigned xid %u",
					  s->transactionId)));

	if (isSubXact)
	{
		Assert(TransactionIdPrecedes(s->parent->transactionId, s->transactionId));
		SubTransSetParent(s->transactionId, s->parent->transactionId, false);
	}

	/*
	 * If it's a top-level transaction, the predicate locking system needs to
	 * be told about it too.
	 */
	if (!isSubXact)
		RegisterPredicateLockingXid(s->transactionId);

	/*
	 * Acquire lock on the transaction XID.  (We assume this cannot block.) We
	 * have to ensure that the lock is assigned to the transaction's own
	 * ResourceOwner.
	 */
	currentOwner = CurrentResourceOwner;
	PG_TRY();
	{
		CurrentResourceOwner = s->curTransactionOwner;
		XactLockTableInsert(s->transactionId);
	}
	PG_CATCH();
	{
		/* Ensure CurrentResourceOwner is restored on error */
		CurrentResourceOwner = currentOwner;
		PG_RE_THROW();
	}
	PG_END_TRY();
	CurrentResourceOwner = currentOwner;

	/*
	 * Every PGPROC_MAX_CACHED_SUBXIDS assigned transaction ids within each
	 * top-level transaction we issue a WAL record for the assignment. We
	 * include the top-level xid and all the subxids that have not yet been
	 * reported using XLOG_XACT_ASSIGNMENT records.
	 *
	 * This is required to limit the amount of shared memory required in a hot
	 * standby server to keep track of in-progress XIDs. See notes for
	 * RecordKnownAssignedTransactionIds().
	 *
	 * We don't keep track of the immediate parent of each subxid, only the
	 * top-level transaction that each subxact belongs to. This is correct in
	 * recovery only because aborted subtransactions are separately WAL
	 * logged.
	 *
	 * This is correct even for the case where several levels above us didn't
	 * have an xid assigned as we recursed up to them beforehand.
	 */
	if (isSubXact && XLogStandbyInfoActive())
	{
		unreportedXids[nUnreportedXids] = s->transactionId;
		nUnreportedXids++;

		/*
		 * ensure this test matches similar one in
		 * RecoverPreparedTransactions()
		 */
		if (nUnreportedXids >= PGPROC_MAX_CACHED_SUBXIDS ||
			log_unknown_top)
		{
			XLogRecData rdata[2];
			xl_xact_assignment xlrec;

			/*
			 * xtop is always set by now because we recurse up transaction
			 * stack to the highest unassigned xid and then come back down
			 */
			xlrec.xtop = GetTopTransactionId();
			Assert(TransactionIdIsValid(xlrec.xtop));
			xlrec.nsubxacts = nUnreportedXids;

			rdata[0].data = (char *) &xlrec;
			rdata[0].len = MinSizeOfXactAssignment;
			rdata[0].buffer = InvalidBuffer;
			rdata[0].next = &rdata[1];

			rdata[1].data = (char *) unreportedXids;
			rdata[1].len = nUnreportedXids * sizeof(TransactionId);
			rdata[1].buffer = InvalidBuffer;
			rdata[1].next = NULL;

			(void) XLogInsert(RM_XACT_ID, XLOG_XACT_ASSIGNMENT, rdata);

			nUnreportedXids = 0;
			/* mark top, not current xact as having been logged */
			TopTransactionStateData.didLogXid = true;
		}
	}
}

/*
 *	GetCurrentSubTransactionId
 */
SubTransactionId
GetCurrentSubTransactionId(void)
{
	TransactionState s = CurrentTransactionState;

	return s->subTransactionId;
}

/*
 *	SubTransactionIsActive
 *
 * Test if the specified subxact ID is still active.  Note caller is
 * responsible for checking whether this ID is relevant to the current xact.
 */
bool
SubTransactionIsActive(SubTransactionId subxid)
{
	TransactionState s;

	for (s = CurrentTransactionState; s != NULL; s = s->parent)
	{
		if (s->state == TRANS_ABORT)
			continue;
		if (s->subTransactionId == subxid)
			return true;
	}
	return false;
}


/*
 *	GetCurrentCommandId
 *
 * "used" must be TRUE if the caller intends to use the command ID to mark
 * inserted/updated/deleted tuples.  FALSE means the ID is being fetched
 * for read-only purposes (ie, as a snapshot validity cutoff).  See
 * CommandCounterIncrement() for discussion.
 */
CommandId
GetCurrentCommandId(bool used)
{
	/* this is global to a transaction, not subtransaction-local */
	if (used)
		currentCommandIdUsed = true;
	return currentCommandId;
}

/*
 *	GetCurrentTransactionStartTimestamp
 */
TimestampTz
GetCurrentTransactionStartTimestamp(void)
{
	return xactStartTimestamp;
}

/*
 *	GetCurrentStatementStartTimestamp
 */
TimestampTz
GetCurrentStatementStartTimestamp(void)
{
	return stmtStartTimestamp;
}

/*
 *	GetCurrentTransactionStopTimestamp
 *
 * We return current time if the transaction stop time hasn't been set
 * (which can happen if we decide we don't need to log an XLOG record).
 */
TimestampTz
GetCurrentTransactionStopTimestamp(void)
{
	if (xactStopTimestamp != 0)
		return xactStopTimestamp;
	return GetCurrentTimestamp();
}

/*
 *	SetCurrentStatementStartTimestamp
 */
void
SetCurrentStatementStartTimestamp(void)
{
	stmtStartTimestamp = GetCurrentTimestamp();
}

/*
 *	SetCurrentTransactionStopTimestamp
 */
static inline void
SetCurrentTransactionStopTimestamp(void)
{
	xactStopTimestamp = GetCurrentTimestamp();
}

/*
 *	GetCurrentTransactionNestLevel
 *
 * Note: this will return zero when not inside any transaction, one when
 * inside a top-level transaction, etc.
 */
int
GetCurrentTransactionNestLevel(void)
{
	TransactionState s = CurrentTransactionState;

	return s->nestingLevel;
}

/*
 * We will return true for the Xid of the current subtransaction, any of
 * its subcommitted children, any of its parents, or any of their
 * previously subcommitted children.  However, a transaction being aborted
 * is no longer "current", even though it may still have an entry on the
 * state stack.
 *
 * The XID of a child is always greater than that of its parent.
 * Taking advantage of this fact simple optimizations are added instead of linear traversal to fasten the search
 *  1] Added fastLink/skipLink pointers to skip nodes in list and scan fast across, instead of visiting all nodes in list
 *  2] Break-out as soon as XID to search is greater than the current node in (parent / child) list
*/
static bool
TransactionIdIsCurrentTransactionIdInternal(TransactionId xid)
{
	TransactionState s = CurrentTransactionState;

	while (s != NULL)
	{
		if ((s->state != TRANS_ABORT) &&
				(TransactionIdIsValid(s->transactionId)))
		{
			int			low,
						high;

			if (TransactionIdEquals(xid, s->transactionId))
				return true;

			/* As the childXids array is ordered, we can use binary search */
			low = 0;
			high = s->nChildXids - 1;
			while (low <= high)
			{
				int				middle;
				TransactionId	probe;

				middle = low + (high - low) / 2;
				probe = s->childXids[middle];
				if (TransactionIdEquals(probe, xid))
					return true;
				else if (TransactionIdPrecedes(probe, xid))
					low = middle + 1;
				else
					high = middle - 1;
			}

			/*
			 * If not found in childXID list and greater than s->transactionId
			 * it cannot be on stack below this node,
			 * as stack is in decreasing order of XIDs
			 * So, can safely breakout.
			 */
			 if (TransactionIdFollows(xid, s->transactionId))
				break;
		}

		if (s->fastLink)
		{
			if (TransactionIdPrecedesOrEquals(xid, s->fastLink->transactionId))
			{
				s = s->fastLink;
				continue;
			}
		}

		s = s->parent;
	}

	return false;
}

/*
 * IsCurrentTransactionIdForReader
 *
 * We can either be a cursor reader or normal reader.
 *
 * The writer_proc will contain all of the subtransaction xids of the current transaction.
 * - case 1: check writer's top transaction id
 * - case 2: if not, check writer's subtransactions
 * - case 3: if overflowed, check topmostxid from pg_subtrans with writer's top transaction id
 */
static
bool IsCurrentTransactionIdForReader(TransactionId xid)
{
	Assert(!Gp_is_writer);

	Assert(SharedLocalSnapshotSlot);

	LWLockAcquire(SharedLocalSnapshotSlot->slotLock, LW_SHARED);

	PGPROC* writer_proc = SharedLocalSnapshotSlot->writer_proc;
	PGXACT* writer_xact = SharedLocalSnapshotSlot->writer_xact;

	if (!writer_proc)
	{
		LWLockRelease(SharedLocalSnapshotSlot->slotLock);
		elog(ERROR, "reference to writer proc not found in shared snapshot");
	}
	else if (!writer_proc->pid)
	{
		LWLockRelease(SharedLocalSnapshotSlot->slotLock);
		elog(ERROR, "writer proc reference shared with reader is invalid");
	}

	TransactionId writer_xid = writer_xact->xid;
	bool overflowed = writer_xact->overflowed;
	bool isCurrent = false;

	if (TransactionIdIsValid(writer_xid))
	{
		/*
		 * Case 1: check top transaction id
		 */
		if (TransactionIdEquals(xid, writer_xid))
		{
			ereportif(Debug_print_full_dtm, LOG,
					  (errmsg("reader encountered writer's top xid %u", xid)));
			isCurrent = true;
		}
		else
		{
			/*
			 * Case 2: check cached subtransaction ids from latest to earliest
			 */
			int subx_index = writer_xact->nxids - 1;
			while (!isCurrent &&  subx_index >= 0)
			{
				isCurrent = TransactionIdEquals(writer_proc->subxids.xids[subx_index], xid);
				subx_index--;
			}
		}
	}

	/* release the lock before accessing pg_subtrans */
	LWLockRelease(SharedLocalSnapshotSlot->slotLock);

	/*
	 * Case 3: if subxids overflowed, check topmostxid of xid from pg_subtrans
	 */
	if (!isCurrent && overflowed)
	{
		Assert(TransactionIdIsValid(writer_xid));
		/*
		 * QE readers don't have access to writer's transaction state.
		 * Therefore, unlike writer, readers have to lookup pg_subtrans, which
		 * is more expensive than searching for an xid in transaction state.  If
		 * xid is older than the oldest running transaction we know of, it is
		 * definitely not current and we can skip pg_subtrans.  Note that
		 * pg_subtrans is not guaranteed to exist for transactions that are
		 * known to be finished.
		 */
		if (TransactionIdFollowsOrEquals(xid, TransactionXmin) &&
			TransactionIdEquals(SubTransGetTopmostTransaction(xid), writer_xid))
		{
			/*
			 * xid is a subtransaction of current transaction.  Did it abort?
			 * If this was a writer, TransactionIdIsCurrentTransactionId()
			 * returns false for aborted subtransactions.  We must therefore
			 * consult clog.  In a writer, this information is available in
			 * CurrentTransactionState.
			 */
			isCurrent = TransactionIdDidAbortForReader(xid) ? false : true;
		}
	}

	ereportif(isCurrent && Debug_print_full_dtm, LOG,
			  (errmsg("reader encountered writer's subxact ID %u", xid)));

	return isCurrent;
}

/*
 *	TransactionIdIsCurrentTransactionId
 */
bool
TransactionIdIsCurrentTransactionId(TransactionId xid)
{
	bool		isCurrentTransactionId = false;

	/*
	 * We always say that BootstrapTransactionId is "not my transaction ID"
	 * even when it is (ie, during bootstrap).  Along with the fact that
	 * transam.c always treats BootstrapTransactionId as already committed,
	 * this causes the tqual.c routines to see all tuples as committed, which
	 * is what we need during bootstrap.  (Bootstrap mode only inserts tuples,
	 * it never updates or deletes them, so all tuples can be presumed good
	 * immediately.)
	 *
	 * Likewise, InvalidTransactionId and FrozenTransactionId are certainly
	 * not my transaction ID, so we can just return "false" immediately for
	 * any non-normal XID.
	 */
	if (!TransactionIdIsNormal(xid))
		return false;

    if (DistributedTransactionContext == DTX_CONTEXT_QE_READER ||
		DistributedTransactionContext == DTX_CONTEXT_QE_ENTRY_DB_SINGLETON)
	{
		isCurrentTransactionId = IsCurrentTransactionIdForReader(xid);

		ereportif(Debug_print_full_dtm, LOG,
				  (errmsg("qExec Reader xid = %u, is current = %s",
						  xid, (isCurrentTransactionId ? "true" : "false"))));

		return isCurrentTransactionId;
	}

	/* we aren't a reader */
	Assert(DistributedTransactionContext != DTX_CONTEXT_QE_ENTRY_DB_SINGLETON);

	return TransactionIdIsCurrentTransactionIdInternal(xid);
}

/*
 *	TransactionStartedDuringRecovery
 *
 * Returns true if the current transaction started while recovery was still
 * in progress. Recovery might have ended since so RecoveryInProgress() might
 * return false already.
 */
bool
TransactionStartedDuringRecovery(void)
{
	return CurrentTransactionState->startedInRecovery;
}

/*
 *	CommandCounterIncrement
 */
void
CommandCounterIncrement(void)
{
	/*
	 * If the current value of the command counter hasn't been "used" to mark
	 * tuples, we need not increment it, since there's no need to distinguish
	 * a read-only command from others.  This helps postpone command counter
	 * overflow, and keeps no-op CommandCounterIncrement operations cheap.
	 */
	if (currentCommandIdUsed)
	{
		currentCommandId += 1;
		if (currentCommandId == InvalidCommandId)
		{
			currentCommandId -= 1;
			ereport(ERROR,
					(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
					 errmsg("cannot have more than 2^32-2 commands in a transaction")));
		}
		currentCommandIdUsed = false;

		/* Propagate new command ID into static snapshots */
		SnapshotSetCommandId(currentCommandId);

		/*
		 * Make any catalog changes done by the just-completed command visible
		 * in the local syscache.  We obviously don't need to do this after a
		 * read-only command.  (But see hacks in inval.c to make real sure we
		 * don't think a command that queued inval messages was read-only.)
		 */
		AtCCI_LocalCache();
	}
}

/*
 * ForceSyncCommit
 *
 * Interface routine to allow commands to force a synchronous commit of the
 * current top-level transaction
 */
void
ForceSyncCommit(void)
{
	forceSyncCommit = true;
}


/* ----------------------------------------------------------------
 *						StartTransaction stuff
 * ----------------------------------------------------------------
 */

/*
 *	AtStart_Cache
 */
static void
AtStart_Cache(void)
{
	AcceptInvalidationMessages();
}

/*
 *	AtStart_Memory
 */
static void
AtStart_Memory(void)
{
	TransactionState s = CurrentTransactionState;

	/*
	 * If this is the first time through, create a private context for
	 * AbortTransaction to work in.  By reserving some space now, we can
	 * insulate AbortTransaction from out-of-memory scenarios.  Like
	 * ErrorContext, we set it up with slow growth rate and a nonzero minimum
	 * size, so that space will be reserved immediately.
	 */
	if (TransactionAbortContext == NULL)
		TransactionAbortContext =
			AllocSetContextCreate(TopMemoryContext,
								  "TransactionAbortContext",
								  32 * 1024,
								  32 * 1024,
								  32 * 1024);

	/*
	 * We shouldn't have a transaction context already.
	 */
	Assert(TopTransactionContext == NULL);

	/*
	 * Create a toplevel context for the transaction.
	 */
	TopTransactionContext =
		AllocSetContextCreate(TopMemoryContext,
							  "TopTransactionContext",
							  ALLOCSET_DEFAULT_MINSIZE,
							  ALLOCSET_DEFAULT_INITSIZE,
							  ALLOCSET_DEFAULT_MAXSIZE);

	/*
	 * In a top-level transaction, CurTransactionContext is the same as
	 * TopTransactionContext.
	 */
	CurTransactionContext = TopTransactionContext;
	s->curTransactionContext = CurTransactionContext;

	/* Make the CurTransactionContext active. */
	MemoryContextSwitchTo(CurTransactionContext);
}

/*
 *	AtStart_ResourceOwner
 */
static void
AtStart_ResourceOwner(void)
{
	TransactionState s = CurrentTransactionState;

	/*
	 * We shouldn't have a transaction resource owner already.
	 */
	Assert(TopTransactionResourceOwner == NULL);

	/*
	 * Create a toplevel resource owner for the transaction.
	 */
	s->curTransactionOwner = ResourceOwnerCreate(NULL, "TopTransaction");

	TopTransactionResourceOwner = s->curTransactionOwner;
	CurTransactionResourceOwner = s->curTransactionOwner;
	CurrentResourceOwner = s->curTransactionOwner;
}

/* ----------------------------------------------------------------
 *						StartSubTransaction stuff
 * ----------------------------------------------------------------
 */

/*
 * AtSubStart_Memory
 */
static void
AtSubStart_Memory(void)
{
	TransactionState s = CurrentTransactionState;

	Assert(CurTransactionContext != NULL);

	/*
	 * Create a CurTransactionContext, which will be used to hold data that
	 * survives subtransaction commit but disappears on subtransaction abort.
	 * We make it a child of the immediate parent's CurTransactionContext.
	 */
	CurTransactionContext = AllocSetContextCreate(CurTransactionContext,
												  "CurTransactionContext",
												  ALLOCSET_DEFAULT_MINSIZE,
												  ALLOCSET_DEFAULT_INITSIZE,
												  ALLOCSET_DEFAULT_MAXSIZE);
	s->curTransactionContext = CurTransactionContext;

	/* Make the CurTransactionContext active. */
	MemoryContextSwitchTo(CurTransactionContext);
}

/*
 * AtSubStart_ResourceOwner
 */
static void
AtSubStart_ResourceOwner(void)
{
	TransactionState s = CurrentTransactionState;

	Assert(s->parent != NULL);

	/*
	 * Create a resource owner for the subtransaction.  We make it a child of
	 * the immediate parent's resource owner.
	 */
	s->curTransactionOwner =
		ResourceOwnerCreate(s->parent->curTransactionOwner,
							"SubTransaction");

	CurTransactionResourceOwner = s->curTransactionOwner;
	CurrentResourceOwner = s->curTransactionOwner;
}

/* ----------------------------------------------------------------
 *						CommitTransaction stuff
 * ----------------------------------------------------------------
 */

/*
 *	RecordTransactionCommit
 *
 * Returns latest XID among xact and its children, or InvalidTransactionId
 * if the xact has no XID.  (We compute that here just because it's easier.)
 */
static TransactionId
RecordTransactionCommit(void)
{
	TransactionId xid;
	bool		markXidCommitted;
	TransactionId latestXid = InvalidTransactionId;
	int			nrels;
	RelFileNodePendingDelete *rels;
	DbDirNode	*deldbs;
	int			ndeldbs;
	int			nchildren;
	TransactionId *children;
	int			nmsgs = 0;
	SharedInvalidationMessage *invalMessages = NULL;
	bool		RelcacheInitFileInval = false;
	bool		wrote_xlog;
	bool		isDtxPrepared = 0;
	bool		isOnePhaseQE = (Gp_role == GP_ROLE_EXECUTE && MyTmGxact->isOnePhaseCommit);
	TMGXACT_LOG gxact_log;
	XLogRecPtr	recptr = InvalidXLogRecPtr;
	DistributedTransactionTimeStamp distribTimeStamp = 0;
	DistributedTransactionId distribXid = 0;
	Oid pending_tablespace_for_deletion = InvalidOid;

	/* Like in CommitTransaction(), treat a QE reader as if there was no XID */
	if (DistributedTransactionContext == DTX_CONTEXT_QE_ENTRY_DB_SINGLETON ||
		DistributedTransactionContext == DTX_CONTEXT_QE_READER)
	{
		xid = InvalidTransactionId;
	}
	else
		xid = GetTopTransactionIdIfAny();
	markXidCommitted = TransactionIdIsValid(xid);

	if (isOnePhaseQE)
		dtxCrackOpenGid(MyTmGxact->gid, &distribTimeStamp, &distribXid);

	/* Get data needed for commit record */
	nrels = smgrGetPendingDeletes(true, &rels);
	ndeldbs = GetPendingDbDeletes(true, &deldbs);
	nchildren = xactGetCommittedChildren(&children);
	if (XLogStandbyInfoActive())
		nmsgs = xactGetCommittedInvalidationMessages(&invalMessages,
													 &RelcacheInitFileInval);
	wrote_xlog = (XactLastRecEnd != 0);

	isDtxPrepared = isPreparedDtxTransaction();

	/*
	 * If we haven't been assigned an XID yet, we neither can, nor do we want
	 * to write a COMMIT record.
	 */
	if (!markXidCommitted)
	{
		/*
		 * We expect that every smgrscheduleunlink is followed by a catalog
		 * update, and hence XID assignment, so we shouldn't get here with any
		 * pending deletes.  Use a real test not just an Assert to check this,
		 * since it's a bit fragile.
		 */
		if (nrels != 0)
			elog(ERROR, "cannot commit a transaction that deleted files but has no xid");

		/* Can't have child XIDs either; AssignTransactionId enforces this */
		Assert(nchildren == 0);

		/*
		 * If we didn't create XLOG entries, we're done here; otherwise we
		 * should trigger flushing those entries the same as a commit record
		 * would.  This will primarily happen for HOT pruning and the like; we
		 * want these to be flushed to disk in due time.
		 */
		if (!isDtxPrepared && !wrote_xlog)
			goto cleanup;
	}

	/*
	 * A QD may write distributed commit record even when it didn't have a
	 * valid local XID if the distributed transaction changed data only on
	 * segments (e.g. DML statement).
	 */
	if (markXidCommitted || isDtxPrepared)
	{
		/*
		 * Begin commit critical section and insert the commit XLOG record.
		 */
		/* Tell bufmgr and smgr to prepare for commit */
		if (markXidCommitted)
			BufmgrCommit();

		if (isDtxPrepared)
			SIMPLE_FAULT_INJECTOR("before_xlog_xact_distributed_commit");

		/*
		 * Mark ourselves as within our "commit critical section".  This
		 * forces any concurrent checkpoint to wait until we've updated
		 * pg_clog.  Without this, it is possible for the checkpoint to set
		 * REDO after the XLOG record but fail to flush the pg_clog update to
		 * disk, leading to loss of the transaction commit if the system
		 * crashes a little later.
		 *
		 * Note: we could, but don't bother to, set this flag in
		 * RecordTransactionAbort.  That's because loss of a transaction abort
		 * is noncritical; the presumption would be that it aborted, anyway.
		 *
		 * It's safe to change the delayChkpt flag of our own backend without
		 * holding the ProcArrayLock, since we're the only one modifying it.
		 * This makes checkpoint's determination of which xacts are delayChkpt
		 * a bit fuzzy, but it doesn't matter.
		 *
		 * In GPDB, if this is a distributed transaction, checkpoint process
		 * should hold off obtaining the REDO pointer while a backend is
		 * writing distributed commit xlog record and changing state of the
		 * distributed transaction.  Otherwise, it is possible that a commit
		 * record is written by a transaction and the checkpointer determines
		 * REDO pointer to be after this commit record.  But the transaction is
		 * yet to change its state to INSERTED_DISRIBUTED_COMMITTED and the
		 * checkpoint process fails to record this transaction in the
		 * checkpoint.  Crash recovery will never see the commit record for
		 * this transaction and the second phase of 2PC will never happen.  The
		 * inCommit flag avoids this situation by blocking checkpointer until a
		 * backend has finished updating the state.
		 */
		START_CRIT_SECTION();
		MyPgXact->delayChkpt = true;

		SetCurrentTransactionStopTimestamp();

		pending_tablespace_for_deletion = GetPendingTablespaceForDeletionForCommit();

		/*
		 * Do we need the long commit record? If not, use the compact format.
		 *
		 * For now always use the non-compact version if wal_level=logical, so
		 * we can hide commits from other databases. TODO: In the future we
		 * should merge compact and non-compact commits and use a flags
		 * variable to determine if it contains subxacts, relations or
		 * invalidation messages, that's more extensible and degrades more
		 * gracefully. Till then, it's just 20 bytes of overhead.
		 *
		 * GPDB: We also always use "non compact" commit record when in a
		 * distributed transaction. (That means, in most cases. But a
		 * distributed commit is so expensive anyway, that this optimization
		 * hardly matters.)
		 */
		if (nrels > 0 || ndeldbs > 0 || nmsgs > 0 || RelcacheInitFileInval ||
			forceSyncCommit || XLogLogicalInfoActive() || isDtxPrepared ||
			OidIsValid(pending_tablespace_for_deletion) || isOnePhaseQE)
		{
			XLogRecData rdata[6];
			int			lastrdata = 0;
			xl_xact_commit xlrec;

			/*
			 * Set flags required for recovery processing of commits.
			 */
			xlrec.xinfo = 0;
			if (RelcacheInitFileInval)
				xlrec.xinfo |= XACT_COMPLETION_UPDATE_RELCACHE_FILE;
			if (forceSyncCommit)
				xlrec.xinfo |= XACT_COMPLETION_FORCE_SYNC_COMMIT;

			xlrec.dbId = MyDatabaseId;
			xlrec.tsId = MyDatabaseTableSpace;

			xlrec.xact_time = xactStopTimestamp;
			xlrec.nrels = nrels;
			xlrec.ndeldbs = ndeldbs;
			xlrec.nsubxacts = nchildren;
			xlrec.nmsgs = nmsgs;
			xlrec.tablespace_oid_to_delete_on_commit =
				GetPendingTablespaceForDeletionForCommit();

			rdata[0].data = (char *) (&xlrec);
			rdata[0].len = MinSizeOfXactCommit;
			rdata[0].buffer = InvalidBuffer;
			/* dump rels to delete */
			if (nrels > 0)
			{
				rdata[0].next = &(rdata[1]);
				rdata[1].data = (char *) rels;
				rdata[1].len = nrels * sizeof(RelFileNodePendingDelete);
				rdata[1].buffer = InvalidBuffer;
				lastrdata = 1;
			}
			/* dump committed child Xids */
			if (nchildren > 0)
			{
				rdata[lastrdata].next = &(rdata[2]);
				rdata[2].data = (char *) children;
				rdata[2].len = nchildren * sizeof(TransactionId);
				rdata[2].buffer = InvalidBuffer;
				lastrdata = 2;
			}
			/* dump shared cache invalidation messages */
			if (nmsgs > 0)
			{
				rdata[lastrdata].next = &(rdata[3]);
				rdata[3].data = (char *) invalMessages;
				rdata[3].len = nmsgs * sizeof(SharedInvalidationMessage);
				rdata[3].buffer = InvalidBuffer;
				lastrdata = 3;
			}
			/* dump dbs to delete */
			if (ndeldbs > 0)
			{
				rdata[lastrdata].next = &(rdata[4]);
				rdata[4].data = (char *) deldbs;
				rdata[4].len = ndeldbs * sizeof(DbDirNode);
				rdata[4].buffer = InvalidBuffer;
				lastrdata = 4;
			}
			rdata[lastrdata].next = NULL;

			SIMPLE_FAULT_INJECTOR("onephase_transaction_commit");

			if (isDtxPrepared)
			{
				/* add global transaction information */
				getDtxLogInfo(&gxact_log);

				rdata[lastrdata].next = &(rdata[5]);
				rdata[5].data = (char *) &gxact_log;
				rdata[5].len = sizeof(gxact_log);
				rdata[5].buffer = InvalidBuffer;
				rdata[5].next = NULL;

				insertingDistributedCommitted();

				/*
				 * MyPgXact->inCommit flag is already set, checkpointer will
				 * be able to see this transaction only after distributed
				 * commit xlog is written and the state is changed.
				 */
				elog(DEBUG5, "writing xlog_xact_distributed_commit");
				recptr = XLogInsert(RM_XACT_ID, XLOG_XACT_DISTRIBUTED_COMMIT, rdata);

				insertedDistributedCommitted();
			}
			else
			{
				xlrec.distribTimeStamp = distribTimeStamp;
				xlrec.distribXid = distribXid;
				recptr = XLogInsert(RM_XACT_ID, XLOG_XACT_COMMIT, rdata);
			}

		}
		else
		{
			XLogRecData rdata[2];
			int			lastrdata = 0;
			xl_xact_commit_compact xlrec;

			xlrec.xact_time = xactStopTimestamp;
			xlrec.nsubxacts = nchildren;
			rdata[0].data = (char *) (&xlrec);
			rdata[0].len = MinSizeOfXactCommitCompact;
			rdata[0].buffer = InvalidBuffer;
			/* dump committed child Xids */
			if (nchildren > 0)
			{
				rdata[0].next = &(rdata[1]);
				rdata[1].data = (char *) children;
				rdata[1].len = nchildren * sizeof(TransactionId);
				rdata[1].buffer = InvalidBuffer;
				lastrdata = 1;
			}
			rdata[lastrdata].next = NULL;

			elog(DEBUG5, "writing xlog_xact_commit_compact");
			recptr = XLogInsert(RM_XACT_ID, XLOG_XACT_COMMIT_COMPACT, rdata);
		}
	}

#ifdef IMPLEMENT_ASYNC_COMMIT
	/*
	 * In PostgreSQL, we can defer flushing XLOG, if the user has set
	 * synchronous_commit = off, and we're not doing cleanup of any non-temp
	 * rels nor committing any command that wanted to force sync commit.
	 *
	 * In GPDB, however, all user transactions need to be committed synchronously,
	 * because we use two-phase commit across the nodes. In order to make GPDB support
	 * async-commit, we also need to implement the temp table detection.
	 */
	if ((wrote_xlog && markXidCommitted &&
		 synchronous_commit > SYNCHRONOUS_COMMIT_OFF) ||
		forceSyncCommit || nrels > 0)
#endif
	{
		XLogFlush(recptr);

#ifdef FAULT_INJECTOR
		if (isDtxPrepared == 0 &&
			CurrentTransactionState->blockState == TBLOCK_END)
		{
			FaultInjector_InjectFaultIfSet("local_tm_record_transaction_commit",
										   DDLNotSpecified,
										   "",  // databaseName
										   ""); // tableName
		}
#endif

		/*
		 * Now we may update the CLOG, if we wrote a COMMIT record above
		 */
		if (markXidCommitted)
		{
			/*
			 * Mark the distributed transaction committed. Note that this
			 * is done *before* updating the clog. As soon as an XID is
			 * marked as comitted in the clog, other backends might try
			 * to look it up in the DistributedLog.
			 */
			/* UNDONE: What are the locking issues here? */
			if (isDtxPrepared)
				DistributedLog_SetCommittedTree(xid, nchildren, children,
												getDtxStartTime(),
												getDistributedTransactionId(),
												/* isRedo */ false);
			else if (isOnePhaseQE)
				DistributedLog_SetCommittedTree(xid, nchildren, children,
												distribTimeStamp, distribXid,
												/* isRedo */ false);

			TransactionIdCommitTree(xid, nchildren, children);
		}
	}
#ifdef IMPLEMENT_ASYNC_COMMIT
	else
	{
		/*
		 * Asynchronous commit case:
		 *
		 * This enables possible committed transaction loss in the case of a
		 * postmaster crash because WAL buffers are left unwritten. Ideally we
		 * could issue the WAL write without the fsync, but some
		 * wal_sync_methods do not allow separate write/fsync.
		 *
		 * Report the latest async commit LSN, so that the WAL writer knows to
		 * flush this commit.
		 */
		XLogSetAsyncXactLSN(XactLastRecEnd);

		/*
		 * We must not immediately update the CLOG, since we didn't flush the
		 * XLOG. Instead, we store the LSN up to which the XLOG must be
		 * flushed before the CLOG may be updated.
		 */
		if (markXidCommitted)
			TransactionIdAsyncCommitTree(xid, nchildren, children, XactLastRecEnd);
	}
#endif

#ifdef FAULT_INJECTOR
	if (isDtxPrepared)
	{
		FaultInjector_InjectFaultIfSet("dtm_xlog_distributed_commit",
									   DDLNotSpecified,
									   "",  // databaseName
									   ""); // tableName
	}
#endif
	
	/*
	 * If we entered a commit critical section, leave it now, and let
	 * checkpoints proceed.
	 */
	if (markXidCommitted || isDtxPrepared)
	{
		MyPgXact->delayChkpt = false;
		END_CRIT_SECTION();
		SIMPLE_FAULT_INJECTOR("after_xlog_xact_distributed_commit");
	}

	/*
	 * Wait for synchronous replication, if required. Similar to the decision
	 * above about using committing asynchronously we only want to wait if
	 * this backend assigned a xid and wrote WAL.  No need to wait if a xid
	 * was assigned due to temporary/unlogged tables or due to HOT pruning.
	 *
	 * Note that at this stage we have marked clog, but still show as running
	 * in the procarray and continue to hold locks.
	 */
	if (markXidCommitted || isDtxPrepared)
	{
		Assert(recptr != 0);
		SyncRepWaitForLSN(recptr);
	}

	/* Compute latestXid while we have the child XIDs handy */
	latestXid = TransactionIdLatest(xid, nchildren, children);

	/* Reset XactLastRecEnd until the next transaction writes something */
	XactLastRecEnd = 0;

cleanup:
	/* And clean up local data */

	return latestXid;
}

/*
 *	RecordDistributedForgetCommitted
 */
void
RecordDistributedForgetCommitted(TMGXACT_LOG *gxact_log)
{
	xl_xact_distributed_forget xlrec;
	XLogRecData rdata[1];

	memcpy(&xlrec.gxact_log, gxact_log, sizeof(TMGXACT_LOG));

	rdata[0].data = (char *) &xlrec;
	rdata[0].len = sizeof(xl_xact_distributed_forget);
	rdata[0].buffer = InvalidBuffer;
	rdata[0].next = NULL;

	XLogInsert(RM_XACT_ID, XLOG_XACT_DISTRIBUTED_FORGET, rdata);
}

/*
 *	AtCCI_LocalCache
 */
static void
AtCCI_LocalCache(void)
{
	/*
	 * Make any pending relation map changes visible.  We must do this before
	 * processing local sinval messages, so that the map changes will get
	 * reflected into the relcache when relcache invals are processed.
	 */
	AtCCI_RelationMap();

	/*
	 * Make catalog changes visible to me for the next command.
	 */
	CommandEndInvalidationMessages();
}

/*
 *	AtCommit_Memory
 */
static void
AtCommit_Memory(void)
{
	/*
	 * Now that we're "out" of a transaction, have the system allocate things
	 * in the top memory context instead of per-transaction contexts.
	 */
	MemoryContextSwitchTo(TopMemoryContext);

	/*
	 * Release all transaction-local memory.
	 */
	Assert(TopTransactionContext != NULL);
	MemoryContextDelete(TopTransactionContext);
	TopTransactionContext = NULL;
	CurTransactionContext = NULL;
	CurrentTransactionState->curTransactionContext = NULL;
}


/* ----------------------------------------------------------------
 *						CommitSubTransaction stuff
 * ----------------------------------------------------------------
 */

/*
 * AtSubCommit_Memory
 */
static void
AtSubCommit_Memory(void)
{
	TransactionState s = CurrentTransactionState;

	Assert(s->parent != NULL);

	/* Return to parent transaction level's memory context. */
	CurTransactionContext = s->parent->curTransactionContext;
	MemoryContextSwitchTo(CurTransactionContext);

	/*
	 * Ordinarily we cannot throw away the child's CurTransactionContext,
	 * since the data it contains will be needed at upper commit.  However, if
	 * there isn't actually anything in it, we can throw it away.  This avoids
	 * a small memory leak in the common case of "trivial" subxacts.
	 */
	if (MemoryContextIsEmpty(s->curTransactionContext))
	{
		MemoryContextDelete(s->curTransactionContext);
		s->curTransactionContext = NULL;
	}
}

/*
 * AtSubCommit_childXids
 *
 * Pass my own XID and my child XIDs up to my parent as committed children.
 */
static void
AtSubCommit_childXids(void)
{
	TransactionState s = CurrentTransactionState;
	int			new_nChildXids;

	Assert(s->parent != NULL);

	/*
	 * The parent childXids array will need to hold my XID and all my
	 * childXids, in addition to the XIDs already there.
	 */
	new_nChildXids = s->parent->nChildXids + s->nChildXids + 1;

	/* Allocate or enlarge the parent array if necessary */
	if (s->parent->maxChildXids < new_nChildXids)
	{
		int			new_maxChildXids;
		TransactionId *new_childXids;

		/*
		 * Make it 2x what's needed right now, to avoid having to enlarge it
		 * repeatedly. But we can't go above MaxAllocSize.  (The latter limit
		 * is what ensures that we don't need to worry about integer overflow
		 * here or in the calculation of new_nChildXids.)
		 */
		new_maxChildXids = Min(new_nChildXids * 2,
							   (int) (MaxAllocSize / sizeof(TransactionId)));

		if (new_maxChildXids < new_nChildXids)
			ereport(ERROR,
					(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
					 errmsg("maximum number of committed subtransactions (%d) exceeded",
							(int) (MaxAllocSize / sizeof(TransactionId)))));

		/*
		 * We keep the child-XID arrays in TopTransactionContext; this avoids
		 * setting up child-transaction contexts for what might be just a few
		 * bytes of grandchild XIDs.
		 */
		if (s->parent->childXids == NULL)
			new_childXids =
				MemoryContextAlloc(TopTransactionContext,
								   new_maxChildXids * sizeof(TransactionId));
		else
			new_childXids = repalloc(s->parent->childXids,
								   new_maxChildXids * sizeof(TransactionId));

		s->parent->childXids = new_childXids;
		s->parent->maxChildXids = new_maxChildXids;
	}

	/*
	 * Copy all my XIDs to parent's array.
	 *
	 * Note: We rely on the fact that the XID of a child always follows that
	 * of its parent.  By copying the XID of this subtransaction before the
	 * XIDs of its children, we ensure that the array stays ordered. Likewise,
	 * all XIDs already in the array belong to subtransactions started and
	 * subcommitted before us, so their XIDs must precede ours.
	 */
	s->parent->childXids[s->parent->nChildXids] = s->transactionId;

	if (s->nChildXids > 0)
		memcpy(&s->parent->childXids[s->parent->nChildXids + 1],
			   s->childXids,
			   s->nChildXids * sizeof(TransactionId));

	s->parent->nChildXids = new_nChildXids;

	/* Release child's array to avoid leakage */
	if (s->childXids != NULL)
		pfree(s->childXids);
	/* We must reset these to avoid double-free if fail later in commit */
	s->childXids = NULL;
	s->nChildXids = 0;
	s->maxChildXids = 0;
}

/* ----------------------------------------------------------------
 *						AbortTransaction stuff
 * ----------------------------------------------------------------
 */

/*
 *	RecordTransactionAbort
 *
 * Returns latest XID among xact and its children, or InvalidTransactionId
 * if the xact has no XID.  (We compute that here just because it's easier.)
 */
static TransactionId
RecordTransactionAbort(bool isSubXact)
{
	TransactionId xid;
	TransactionId latestXid;
	int			nrels;
	RelFileNodePendingDelete *rels;
	int			nchildren;
	TransactionId *children;
	DbDirNode	*deldbs;
	int			ndeldbs;
	XLogRecData rdata[4];
	int			lastrdata = 0;
	xl_xact_abort xlrec;
	bool		isQEReader;

	/* Like in CommitTransaction(), treat a QE reader as if there was no XID */
	isQEReader = (DistributedTransactionContext == DTX_CONTEXT_QE_READER ||
					DistributedTransactionContext == DTX_CONTEXT_QE_ENTRY_DB_SINGLETON);
	/*
	 * Also, if an error occurred during distributed COMMIT processing, and
	 * we had already decided that we are going to commit this transaction and
	 * wrote a commit record for it, there's no turning back. The Distributed
	 * Transaction Manager will take care of completing the transaction for us.
	 */
	if (isQEReader || getCurrentDtxState() == DTX_STATE_NOTIFYING_COMMIT_PREPARED)
		xid = InvalidTransactionId;
	else
		xid = GetCurrentTransactionIdIfAny();

	/*
	 * If we haven't been assigned an XID, nobody will care whether we aborted
	 * or not.  Hence, we're done in that case.  It does not matter if we have
	 * rels to delete (note that this routine is not responsible for actually
	 * deleting 'em).  We cannot have any child XIDs, either.
	 */
	SetCurrentTransactionStopTimestamp();
	if (!TransactionIdIsValid(xid))
	{
		/* Reset XactLastRecEnd until the next transaction writes something */
		if (!isSubXact)
			XactLastRecEnd = 0;
		return InvalidTransactionId;
	}

	/*
	 * We have a valid XID, so we should write an ABORT record for it.
	 *
	 * We do not flush XLOG to disk here, since the default assumption after a
	 * crash would be that we aborted, anyway.  For the same reason, we don't
	 * need to worry about interlocking against checkpoint start.
	 */

	/*
	 * Check that we haven't aborted halfway through RecordTransactionCommit.
	 */
	if (TransactionIdDidCommit(xid))
		elog(PANIC, "cannot abort transaction %u, it was already committed",
			 xid);

	/* Fetch the data we need for the abort record */
	nrels = smgrGetPendingDeletes(false, &rels);
	ndeldbs = GetPendingDbDeletes(false, &deldbs);
	nchildren = xactGetCommittedChildren(&children);

	/* XXX do we really need a critical section here? */
	START_CRIT_SECTION();

	/* Write the ABORT record */
	if (isSubXact)
		xlrec.xact_time = GetCurrentTimestamp();
	else
	{
		SetCurrentTransactionStopTimestamp();
		xlrec.xact_time = xactStopTimestamp;
	}
	xlrec.tablespace_oid_to_delete_on_abort = GetPendingTablespaceForDeletionForAbort();
	xlrec.nrels = nrels;
	xlrec.ndeldbs = ndeldbs;
	xlrec.nsubxacts = nchildren;
	rdata[0].data = (char *) (&xlrec);
	rdata[0].len = MinSizeOfXactAbort;
	rdata[0].buffer = InvalidBuffer;
	/* dump rels to delete */
	if (nrels > 0)
	{
		rdata[0].next = &(rdata[1]);
		rdata[1].data = (char *) rels;
		rdata[1].len = nrels * sizeof(RelFileNodePendingDelete);
		rdata[1].buffer = InvalidBuffer;
		lastrdata = 1;
	}
	/* dump committed child Xids */
	if (nchildren > 0)
	{
		rdata[lastrdata].next = &(rdata[2]);
		rdata[2].data = (char *) children;
		rdata[2].len = nchildren * sizeof(TransactionId);
		rdata[2].buffer = InvalidBuffer;
		lastrdata = 2;
	}
	/* dump dbs to delete */
	if (ndeldbs > 0)
	{
		rdata[lastrdata].next = &(rdata[3]);
		rdata[3].data = (char *) deldbs;
		rdata[3].len = ndeldbs * sizeof(DbDirNode);
		rdata[3].buffer = InvalidBuffer;
		lastrdata = 3;
	}
	rdata[lastrdata].next = NULL;

	(void) XLogInsert(RM_XACT_ID, XLOG_XACT_ABORT, rdata);

	/*
	 * Report the latest async abort LSN, so that the WAL writer knows to
	 * flush this abort. There's nothing to be gained by delaying this, since
	 * WALWriter may as well do this when it can. This is important with
	 * streaming replication because if we don't flush WAL regularly we will
	 * find that large aborts leave us with a long backlog for when commits
	 * occur after the abort, increasing our window of data loss should
	 * problems occur at that point.
	 */
	if (!isSubXact)
		XLogSetAsyncXactLSN(XactLastRecEnd);

	/*
	 * Mark the transaction aborted in clog.  This is not absolutely necessary
	 * but we may as well do it while we are here; also, in the subxact case
	 * it is helpful because XactLockTableWait makes use of it to avoid
	 * waiting for already-aborted subtransactions.  It is OK to do it without
	 * having flushed the ABORT record to disk, because in event of a crash
	 * we'd be assumed to have aborted anyway.
	 */
	TransactionIdAbortTree(xid, nchildren, children);

	END_CRIT_SECTION();

	/* Compute latestXid while we have the child XIDs handy */
	latestXid = TransactionIdLatest(xid, nchildren, children);

	/*
	 * If we're aborting a subtransaction, we can immediately remove failed
	 * XIDs from PGPROC's cache of running child XIDs.  We do that here for
	 * subxacts, because we already have the child XID array at hand.  For
	 * main xacts, the equivalent happens just after this function returns.
	 */
	if (isSubXact)
		XidCacheRemoveRunningXids(xid, nchildren, children, latestXid);

	/* Reset XactLastRecEnd until the next transaction writes something */
	if (!isSubXact)
		XactLastRecEnd = 0;

	if (max_wal_senders > 0)
		WalSndWakeup();

	return latestXid;
}

/*
 *	AtAbort_Memory
 */
static void
AtAbort_Memory(void)
{
	/*
	 * Switch into TransactionAbortContext, which should have some free space
	 * even if nothing else does.  We'll work in this context until we've
	 * finished cleaning up.
	 *
	 * It is barely possible to get here when we've not been able to create
	 * TransactionAbortContext yet; if so use TopMemoryContext.
	 */
	if (TransactionAbortContext != NULL)
		MemoryContextSwitchTo(TransactionAbortContext);
	else
		MemoryContextSwitchTo(TopMemoryContext);
}

/*
 * AtSubAbort_Memory
 */
static void
AtSubAbort_Memory(void)
{
	Assert(TransactionAbortContext != NULL);

	MemoryContextSwitchTo(TransactionAbortContext);
}


/*
 *	AtAbort_ResourceOwner
 */
static void
AtAbort_ResourceOwner(void)
{
	/*
	 * Make sure we have a valid ResourceOwner, if possible (else it will be
	 * NULL, which is OK)
	 */
	CurrentResourceOwner = TopTransactionResourceOwner;
}

/*
 * AtSubAbort_ResourceOwner
 */
static void
AtSubAbort_ResourceOwner(void)
{
	TransactionState s = CurrentTransactionState;

	/* Make sure we have a valid ResourceOwner */
	CurrentResourceOwner = s->curTransactionOwner;
}


/*
 * AtSubAbort_childXids
 */
static void
AtSubAbort_childXids(void)
{
	TransactionState s = CurrentTransactionState;

	/*
	 * We keep the child-XID arrays in TopTransactionContext (see
	 * AtSubCommit_childXids).  This means we'd better free the array
	 * explicitly at abort to avoid leakage.
	 */
	if (s->childXids != NULL)
		pfree(s->childXids);
	s->childXids = NULL;
	s->nChildXids = 0;
	s->maxChildXids = 0;

	/*
	 * We could prune the unreportedXids array here. But we don't bother. That
	 * would potentially reduce number of XLOG_XACT_ASSIGNMENT records but it
	 * would likely introduce more CPU time into the more common paths, so we
	 * choose not to do that.
	 */
}

/* ----------------------------------------------------------------
 *						CleanupTransaction stuff
 * ----------------------------------------------------------------
 */

/*
 *	AtCleanup_Memory
 */
static void
AtCleanup_Memory(void)
{
	Assert(CurrentTransactionState->parent == NULL);

	/*
	 * Now that we're "out" of a transaction, have the system allocate things
	 * in the top memory context instead of per-transaction contexts.
	 */
	MemoryContextSwitchTo(TopMemoryContext);

	/*
	 * Clear the special abort context for next time.
	 */
	if (TransactionAbortContext != NULL)
		MemoryContextResetAndDeleteChildren(TransactionAbortContext);

	/*
	 * Release all transaction-local memory.
	 */
	if (TopTransactionContext != NULL)
		MemoryContextDelete(TopTransactionContext);
	TopTransactionContext = NULL;
	CurTransactionContext = NULL;
	CurrentTransactionState->curTransactionContext = NULL;
}


/* ----------------------------------------------------------------
 *						CleanupSubTransaction stuff
 * ----------------------------------------------------------------
 */

/*
 * AtSubCleanup_Memory
 */
static void
AtSubCleanup_Memory(void)
{
	TransactionState s = CurrentTransactionState;

	Assert(s->parent != NULL);

	/* Make sure we're not in an about-to-be-deleted context */
	MemoryContextSwitchTo(s->parent->curTransactionContext);
	CurTransactionContext = s->parent->curTransactionContext;

	/*
	 * Clear the special abort context for next time.
	 */
	if (TransactionAbortContext != NULL)
		MemoryContextResetAndDeleteChildren(TransactionAbortContext);

	/*
	 * Delete the subxact local memory contexts. Its CurTransactionContext can
	 * go too (note this also kills CurTransactionContexts from any children
	 * of the subxact).
	 */
	if (s->curTransactionContext)
		MemoryContextDelete(s->curTransactionContext);
	s->curTransactionContext = NULL;
}

/* ----------------------------------------------------------------
 *						interface routines
 * ----------------------------------------------------------------
 */
/* MPP routine for setting the transaction id.	this is needed for the shared
 * snapshot for segmates.
 *
 * TODO: this sucks to have to allow this since its potentially very dangerous.
 * maybe we can re-factor the shared snapshot stuff differently to fix this.
 * but unfortunately the XID and snapshot info is kept in two entirely different
 * places so it doesn't look good.
 *
 * DOH: this totally ignores subtransactions for now!
 */
void
SetSharedTransactionId_writer(DtxContext distributedTransactionContext)
{
	Assert(SharedLocalSnapshotSlot != NULL);
	Assert(LWLockHeldByMe(SharedLocalSnapshotSlot->slotLock));

	Assert(distributedTransactionContext == DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE ||
		   distributedTransactionContext == DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER ||
		   distributedTransactionContext == DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER ||
		   distributedTransactionContext == DTX_CONTEXT_QE_AUTO_COMMIT_IMPLICIT);

	ereportif(Debug_print_full_dtm, LOG,
			  (errmsg("%s setting shared xid %u -> %u",
					  DtxContextToString(distributedTransactionContext),
					  SharedLocalSnapshotSlot->xid,
					  TopTransactionStateData.transactionId)));
	SharedLocalSnapshotSlot->xid = TopTransactionStateData.transactionId;
}

void
SetSharedTransactionId_reader(TransactionId xid, CommandId cid, DtxContext distributedTransactionContext)
{
	Assert(distributedTransactionContext == DTX_CONTEXT_QE_READER ||
		   distributedTransactionContext == DTX_CONTEXT_QE_ENTRY_DB_SINGLETON);

	/*
	 * For DTX_CONTEXT_QE_READER or DTX_CONTEXT_QE_ENTRY_DB_SINGLETON, during
	 * StartTransaction(), currently we temporarily set the
	 * TopTransactionStateData.transactionId to what we find that time in
	 * SharedLocalSnapshot slot. Since, then QE writer could have moved-on and
	 * hence we reset the same to update to correct value here.
	 */
	TopTransactionStateData.transactionId = xid;
	currentCommandId = cid;
	ereportif(Debug_print_full_dtm, LOG,
			  (errmsg("qExec READER setting local xid=%u, cid=%u "
					  "(distributedXid %u/%u)",
					  TopTransactionStateData.transactionId, currentCommandId,
					  QEDtxContextInfo.distributedXid,
					  QEDtxContextInfo.segmateSync)));
}

/*
 *	StartTransaction
 */
static void
StartTransaction(void)
{
	TransactionState s;
	VirtualTransactionId vxid;

	if (DistributedTransactionContext == DTX_CONTEXT_QE_ENTRY_DB_SINGLETON)
	{
		SIMPLE_FAULT_INJECTOR("transaction_start_under_entry_db_singleton");
	}

	/*
	 * Let's just make sure the state stack is empty
	 */
	s = &TopTransactionStateData;
	CurrentTransactionState = s;

	/* check the current transaction state */
	Assert(s->state == TRANS_DEFAULT);

	/*
	 * Set the current transaction state information appropriately during
	 * start processing.  Note that once the transaction status is switched
	 * this process cannot fail until the user ID and the security context
	 * flags are fetched below.
	 */
	s->state = TRANS_START;
	s->transactionId = InvalidTransactionId;	/* until assigned */

	/*
	 * initialize current transaction state fields
	 *
	 * note: prevXactReadOnly is not used at the outermost level
	 */
	s->nestingLevel = 1;
	s->gucNestLevel = 1;
	s->childXids = NULL;
	s->nChildXids = 0;
	s->maxChildXids = 0;

	/*
	 * Once the current user ID and the security context flags are fetched,
	 * both will be properly reset even if transaction startup fails.
	 */
	GetUserIdAndSecContext(&s->prevUser, &s->prevSecContext);

	/* SecurityRestrictionContext should never be set outside a transaction */
	Assert(s->prevSecContext == 0);

	/*
	 * Make sure we've reset xact state variables
	 *
	 * If recovery is still in progress, mark this transaction as read-only.
	 * We have lower level defences in XLogInsert and elsewhere to stop us
	 * from modifying data during recovery, but this gives the normal
	 * indication to the user that the transaction is read-only.
	 */
	if (RecoveryInProgress())
	{
		s->startedInRecovery = true;
		XactReadOnly = true;
	}
	else
	{
		s->startedInRecovery = false;
		XactReadOnly = DefaultXactReadOnly;
	}
	XactDeferrable = DefaultXactDeferrable;
	XactIsoLevel = DefaultXactIsoLevel;
	forceSyncCommit = false;
	/* Disabled in GPDB as per comment in PrepareTransaction(). */
#if 0
	MyXactAccessedTempRel = false;
#endif
	seqXlogWrite = false;

	/*
	 * reinitialize within-transaction counters
	 */
	s->subTransactionId = TopSubTransactionId;
	currentSubTransactionId = TopSubTransactionId;
	currentCommandId = FirstCommandId;
	currentCommandIdUsed = false;
	currentSavepointTotal = 0;

	fastNodeCount = 0;
	previousFastLink = NULL;

	/*
	 * initialize reported xid accounting
	 */
	nUnreportedXids = 0;
	s->didLogXid = false;
	s->executorDidWriteXLog = false;

	/*
	 * must initialize resource-management stuff first
	 */
	AtStart_Memory();
	AtStart_ResourceOwner();

	/*
	 * Transactions may be started while recovery is in progress, if
	 * hot standby is enabled.  This mode is not supported in
	 * Greenplum yet.
	 */
	AssertImply(DistributedTransactionContext != DTX_CONTEXT_LOCAL_ONLY,
				!s->startedInRecovery);
	/*
	 * MPP Modification
	 *
	 * If we're an executor and don't have a valid QDSentXID, then we're starting
	 * a purely-local transaction.
	 */
	switch (DistributedTransactionContext)
	{
		case DTX_CONTEXT_LOCAL_ONLY:
		case DTX_CONTEXT_QD_RETRY_PHASE_2:
		case DTX_CONTEXT_QE_FINISH_PREPARED:
		{
			/*
			 * MPP: we're in utility-mode or a QE starting a pure-local
			 * transaction without any synchronization to segmates!
			 * (e.g. CatchupInterruptHandler)
			 */
		}
		break;

		case DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE:
		{
			/*
			 * Generate the distributed transaction ID and save it.
			 * it's not really needed by a select-only implicit transaction, but
			 * currently gpfdist and pxf is using it.
			 * We should probably replace xid with "session id + command id" in
			 * identify a query in gpfdist and pxf.
			 */
			currentDistribXid = generateGID();

			if (SharedLocalSnapshotSlot != NULL)
			{
				LWLockAcquire(SharedLocalSnapshotSlot->slotLock, LW_EXCLUSIVE);
				ereportif(Debug_print_full_dtm, LOG,
						  (errmsg("setting shared snapshot startTimestamp = "
								  INT64_FORMAT "[old=" INT64_FORMAT "])",
								  stmtStartTimestamp,
								  SharedLocalSnapshotSlot->startTimestamp)));
				SharedLocalSnapshotSlot->startTimestamp = stmtStartTimestamp;
				LWLockRelease(SharedLocalSnapshotSlot->slotLock);
			}
		}
		break;

		case DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER:
		case DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER:
		case DTX_CONTEXT_QE_AUTO_COMMIT_IMPLICIT:
		{
			/* If we're running in test-mode insert a delay in writer. */
			if (gp_enable_slow_writer_testmode)
				pg_usleep(500000);

			if (DistributedTransactionContext != DTX_CONTEXT_QE_AUTO_COMMIT_IMPLICIT &&
				QEDtxContextInfo.distributedXid == InvalidDistributedTransactionId)
			{
				elog(ERROR,
					 "distributed transaction id is invalid in context %s",
					 DtxContextToString(DistributedTransactionContext));
			}

			/*
			 * Snapshot must not be created before setting transaction
			 * isolation level.
			 */
			Assert(!FirstSnapshotSet);

			/* Assume transaction characteristics as sent by QD */
			XactIsoLevel = mppTxOptions_IsoLevel(
				QEDtxContextInfo.distributedTxnOptions);
			XactReadOnly = isMppTxOptions_ReadOnly(
				QEDtxContextInfo.distributedTxnOptions);

			/*
			 * MPP: we're a QE Writer.
			 *
			 * For DTX_CONTEXT_QE_AUTO_COMMIT_IMPLICIT don't use the
			 * distributed xid map since this may be one of funny distributed
			 * queries the executor uses behind the scenes for estimation
			 * work. We also don't need a local XID right now - we let it be
			 * assigned lazily, as on a local transaction. This transaction
			 * will auto-commit, and then we will follow it with the real user
			 * command.
			 */
			if (DistributedTransactionContext ==
				DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER ||
				DistributedTransactionContext ==
				DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER)
			{
				currentDistribXid = QEDtxContextInfo.distributedXid;

				Assert(QEDtxContextInfo.distributedTimeStamp != 0);
				Assert(QEDtxContextInfo.distributedXid !=
					   InvalidDistributedTransactionId);

				/*
				 * Update distributed XID info, this is only used for
				 * debugging.
				 */
				LocalDistribXactData *ele = &MyProc->localDistribXactData;
				ele->distribTimeStamp = QEDtxContextInfo.distributedTimeStamp;
				ele->distribXid = QEDtxContextInfo.distributedXid;
				ele->state = LOCALDISTRIBXACT_STATE_ACTIVE;
			}

			if (SharedLocalSnapshotSlot != NULL)
			{
				LWLockAcquire(SharedLocalSnapshotSlot->slotLock, LW_EXCLUSIVE);

				SharedLocalSnapshotSlot->ready = false;
				SharedLocalSnapshotSlot->xid = s->transactionId;
				SharedLocalSnapshotSlot->startTimestamp = stmtStartTimestamp;
				SharedLocalSnapshotSlot->QDxid = QEDtxContextInfo.distributedXid;
				SharedLocalSnapshotSlot->pid = MyProc->pid;
				SharedLocalSnapshotSlot->writer_proc = MyProc;
				SharedLocalSnapshotSlot->writer_xact = MyPgXact;

				ereportif(Debug_print_full_dtm, LOG,
						  (errmsg(
							  "qExec writer setting distributedXid: %d "
							  "sharedQDxid %d (shared xid %u -> %u) ready %s"
							  " (shared timeStamp = " INT64_FORMAT " -> "
							  INT64_FORMAT ")",
							  QEDtxContextInfo.distributedXid,
							  SharedLocalSnapshotSlot->QDxid,
							  SharedLocalSnapshotSlot->xid,
							  s->transactionId,
							  SharedLocalSnapshotSlot->ready ? "true" : "false",
							  SharedLocalSnapshotSlot->startTimestamp,
							  xactStartTimestamp)));
				LWLockRelease(SharedLocalSnapshotSlot->slotLock);
			}
		}
		break;

		case DTX_CONTEXT_QE_ENTRY_DB_SINGLETON:
		case DTX_CONTEXT_QE_READER:
		{
			/*
			 * MPP: we're a QE Reader.
			 */
			Assert (SharedLocalSnapshotSlot != NULL);
			currentDistribXid = QEDtxContextInfo.distributedXid;

			/*
			 * Snapshot must not be created before setting transaction
			 * isolation level.
			 */
			Assert(!FirstSnapshotSet);

			/* Assume transaction characteristics as sent by QD */
			XactIsoLevel = mppTxOptions_IsoLevel(
				QEDtxContextInfo.distributedTxnOptions);
			XactReadOnly = isMppTxOptions_ReadOnly(
				QEDtxContextInfo.distributedTxnOptions);

			ereportif(Debug_print_full_dtm, LOG,
					  (errmsg("qExec reader: distributedXid %d currcid %d "
							  "gxid = %u DtxContext '%s' sharedsnapshots: %s",
							  QEDtxContextInfo.distributedXid,
							  QEDtxContextInfo.curcid,
							  getDistributedTransactionId(),
							  DtxContextToString(DistributedTransactionContext),
							  SharedSnapshotDump())));
		}
		break;
	
		case DTX_CONTEXT_QE_PREPARED:
			elog(FATAL, "Unexpected segment distribute transaction context: '%s'",
				 DtxContextToString(DistributedTransactionContext));
			break;
	
		default:
			elog(PANIC, "Unrecognized DTX transaction context: %d",
				 (int) DistributedTransactionContext);
			break;
	}

	ereportif(Debug_print_snapshot_dtm, LOG,
			  (errmsg("[Distributed Snapshot #%u] *StartTransaction* "
					  "(gxid = %u, xid = %u, '%s')",
					  (!FirstSnapshotSet ? 0 :
					   GetTransactionSnapshot()->
					   distribSnapshotWithLocalMapping.ds.distribSnapshotId),
					  getDistributedTransactionId(),
					  s->transactionId,
					  DtxContextToString(DistributedTransactionContext))));

	/*
	 * Assign a new LocalTransactionId, and combine it with the backendId to
	 * form a virtual transaction id.
	 */
	vxid.backendId = MyBackendId;
	vxid.localTransactionId = GetNextLocalTransactionId();

	/*
	 * Lock the virtual transaction id before we announce it in the proc array
	 */
	VirtualXactLockTableInsert(vxid);

	/*
	 * Advertise it in the proc array.  We assume assignment of
	 * LocalTransactionID is atomic, and the backendId should be set already.
	 */
	Assert(MyProc->backendId == vxid.backendId);
	MyProc->lxid = vxid.localTransactionId;

	TRACE_POSTGRESQL_TRANSACTION_START(vxid.localTransactionId);

	/*
	 * set transaction_timestamp() (a/k/a now()).  We want this to be the same
	 * as the first command's statement_timestamp(), so don't do a fresh
	 * GetCurrentTimestamp() call (which'd be expensive anyway).  Also, mark
	 * xactStopTimestamp as unset.
	 */
	xactStartTimestamp = stmtStartTimestamp;
	xactStopTimestamp = 0;
	pgstat_report_xact_timestamp(xactStartTimestamp);

	/*
	 * initialize other subsystems for new transaction
	 */
	AtStart_GUC();
	AtStart_Inval();
	AtStart_Cache();
	AfterTriggerBeginXact();

	/*
	 * done with start processing, set current transaction state to "in
	 * progress"
	 */
	s->state = TRANS_INPROGRESS;

	/*
	 * Update the snapshot of gp_segment_configuration, it's not changed
	 * until the end of transaction, do this update inside a transaction
	 * because it does a catalog lookup.
	 *
	 * Sometimes, a new transaction is started before first access to db,
	 * however, reading a catalog like gp_segment_configuration need a
	 * database be selected. In such case, we disallow updating the snapshot
	 * of segments configuration.
	 */
	if (Gp_role == GP_ROLE_DISPATCH && OidIsValid(MyDatabaseId))
		cdbcomponent_updateCdbComponents();

	/*
	 * Acquire a resource group slot.
	 *
	 * Slot is successfully acquired when AssignResGroupOnMaster() is returned.
	 * This slot will be released when the transaction is committed or aborted.
	 *
	 * Note that AssignResGroupOnMaster() can throw a PG exception. Since we
	 * have set the transaction state to TRANS_INPROGRESS by this point, any
	 * exceptions thrown will trigger AbortTransaction() and free the slot.
	 *
	 * It's important that we acquire the resource group *after* starting the
	 * transaction (i.e. setting up the per-transaction memory context).
	 * As part of determining the resource group that the transaction should be
	 * assigned to, AssignResGroupOnMaster() accesses pg_authid, and a
	 * transaction should be in progress when it does so.
	 */
	if (ShouldAssignResGroupOnMaster())
		AssignResGroupOnMaster();

	ShowTransactionState("StartTransaction");

	ereportif(Debug_print_full_dtm, LOG,
			  (errmsg("StartTransaction in DTX Context = '%s', "
					  "isolation level %s, read-only = %d, %s",
					  DtxContextToString(DistributedTransactionContext),
					  IsoLevelAsUpperString(XactIsoLevel), XactReadOnly,
					  LocalDistribXact_DisplayString(MyProc->pgprocno))));
}

/*
 *	CommitTransaction
 *
 * NB: if you change this routine, better look at PrepareTransaction too!
 */
static void
CommitTransaction(void)
{
	TransactionState s = CurrentTransactionState;
	TransactionId latestXid;

	ShowTransactionState("CommitTransaction");

	/*
	 * check the current transaction state
	 */
	if (s->state != TRANS_INPROGRESS)
		elog(WARNING, "CommitTransaction while in %s state",
			 TransStateAsString(s->state));
	Assert(s->parent == NULL);

	if (Gp_role == GP_ROLE_EXECUTE && !Gp_is_writer)
		elog(DEBUG1,"CommitTransaction: called as segment Reader");

	/*
	 * Do pre-commit processing that involves calling user-defined code, such
	 * as triggers.  Since closing cursors could queue trigger actions,
	 * triggers could open cursors, etc, we have to keep looping until there's
	 * nothing left to do.
	 */
	for (;;)
	{
		/*
		 * Fire all currently pending deferred triggers.
		 */
		AfterTriggerFireDeferred();

		/*
		 * Close open portals (converting holdable ones into static portals).
		 * If there weren't any, we are done ... otherwise loop back to check
		 * if they queued deferred triggers.  Lather, rinse, repeat.
		 */
		if (!PreCommit_Portals(false))
			break;
	}

	CallXactCallbacks(XACT_EVENT_PRE_COMMIT);

	/*
	 * The remaining actions cannot call any user-defined code, so it's safe
	 * to start shutting down within-transaction services.  But note that most
	 * of this stuff could still throw an error, which would switch us into
	 * the transaction-abort path.
	 */

	/* Shut down the deferred-trigger manager */
	AfterTriggerEndXact(true);

	AtEOXact_SharedSnapshot();

	/* Perform any Resource Scheduler commit procesing. */
	if (Gp_role == GP_ROLE_DISPATCH && IsResQueueEnabled())
		AtCommit_ResScheduler();

	/* Perform any AO table commit processing */
	AtCommit_AppendOnly();

	/*
	 * Let ON COMMIT management do its thing (must happen after closing
	 * cursors, to avoid dangling-reference problems)
	 */
	PreCommit_on_commit_actions();

	/* This can still fail */
	AtEOXact_DispatchOids(true);

	/* close large objects before lower-level cleanup */
	AtEOXact_LargeObject(true);

	/*
	 * Mark serializable transaction as complete for predicate locking
	 * purposes.  This should be done as late as we can put it and still allow
	 * errors to be raised for failure patterns found at commit.
	 */
	PreCommit_CheckForSerializationFailure();

	/*
	 * Insert notifications sent by NOTIFY commands into the queue.  This
	 * should be late in the pre-commit sequence to minimize time spent
	 * holding the notify-insertion lock.
	 */
	PreCommit_Notify();

	/*
	 * Prepare all QE.
	 */
	prepareDtxTransaction();

#ifdef FAULT_INJECTOR
	if (isPreparedDtxTransaction())
	{
		FaultInjector_InjectFaultIfSet(
									   "transaction_abort_after_distributed_prepared",
									   DDLNotSpecified,
									   "",	// databaseName
									   ""); // tableName
	}
#endif	

	if (Debug_abort_after_distributed_prepared &&
		isPreparedDtxTransaction())
	{
		ereport(ERROR,
				(errcode(ERRCODE_FAULT_INJECT),
				 errmsg("Raise an error as directed by Debug_abort_after_distributed_prepared")));
	}

	/* Prevent cancel/die interrupt while cleaning up */
	HOLD_INTERRUPTS();

	/* Commit updates to the relation map --- do this as late as possible */
	AtEOXact_RelationMap(true);

	/*
	 * set the current transaction state information appropriately during
	 * commit processing
	 */
	s->state = TRANS_COMMIT;

	/*
	 * Here is where we really truly commit.
	 */
	latestXid = RecordTransactionCommit();

	TRACE_POSTGRESQL_TRANSACTION_COMMIT(MyProc->lxid);

	EndLocalDistribXact(true);

	if (notifyCommittedDtxTransactionIsNeeded())
	{
		/*
		 * Do 2nd phase of commit to all QE. NOTE: we can't process
		 * signals (which may attempt to abort our now partially-completed
		 * transaction) until we've notified the QEs.
		 *
		 * Very important that PGPROC still thinks the transaction is still in progress so
		 * SnapshotNow reader don't jump to the conclusion this distributed transaction is
		 * finished.  So, notifyCommittedDtxTransaction will take responsbility to clear
		 * PGPROC under the ProcArrayLock after the broadcast.  MPP-16087.
		 *
		 * And, that we have not master released locks, yet, too.
		 *
		 * Note:  do this BEFORE clearing the resource owner, as the dispatch
		 * routines might want to use them.  Plus, we want AtCommit_Memory to 
		 * happen after using the dispatcher.
		 */
		notifyCommittedDtxTransaction();

		/*
		 * Clear both local and distributed transaction states.
		 */
		ClearTransactionState(latestXid);
	}
	else
	{
		/*
		 * Let others know about no transaction in progress by me. Note that this
		 * must be done _before_ releasing locks we hold and _after_
		 * RecordTransactionCommit.
		 */
		ProcArrayEndTransaction(MyProc, latestXid, false);
	}

	/*
	 * This is all post-commit cleanup.  Note that if an error is raised here,
	 * it's too late to abort the transaction.  This should be just
	 * noncritical resource releasing.
	 *
	 * The ordering of operations is not entirely random.  The idea is:
	 * release resources visible to other backends (eg, files, buffer pins);
	 * then release locks; then release backend-local resources. We want to
	 * release locks at the point where any backend waiting for us will see
	 * our transaction as being fully cleaned up.
	 *
	 * Resources that can be associated with individual queries are handled by
	 * the ResourceOwner mechanism.  The other calls here are for backend-wide
	 * state.
	 */

	CallXactCallbacks(XACT_EVENT_COMMIT);
	CallXactCallbacksOnce(XACT_EVENT_COMMIT);

	ResourceOwnerRelease(TopTransactionResourceOwner,
						 RESOURCE_RELEASE_BEFORE_LOCKS,
						 true, true);

	/* Check we've released all buffer pins */
	AtEOXact_Buffers(true);

	/* Clean up the relation cache */
	AtEOXact_RelationCache(true);

	/*
	 * Make catalog changes visible to all backends.  This has to happen after
	 * relcache references are dropped (see comments for
	 * AtEOXact_RelationCache), but before locks are released (if anyone is
	 * waiting for lock on a relation we've modified, we want them to know
	 * about the catalog change before they start using the relation).
	 */
	AtEOXact_Inval(true);

	AtEOXact_MultiXact();

	ResourceOwnerRelease(TopTransactionResourceOwner,
						 RESOURCE_RELEASE_LOCKS,
						 true, true);
	ResourceOwnerRelease(TopTransactionResourceOwner,
						 RESOURCE_RELEASE_AFTER_LOCKS,
						 true, true);

	/*
	 * Likewise, dropping of files deleted during the transaction is best done
	 * after releasing relcache and buffer pins.  (This is not strictly
	 * necessary during commit, since such pins should have been released
	 * already, but this ordering is definitely critical during abort.)  Since
	 * this may take many seconds, also delay until after releasing locks.
	 * Other backends will observe the attendant catalog changes and not
	 * attempt to access affected files.
	 *
	 * Same considerations for tablespace deletion
	 */
	smgrDoPendingDeletes(true);
	DoPendingDbDeletes(true);

	/*
	 * Only QD holds the session level lock this long for a movedb operation.
	 * This is to prevent another transaction from moving database objects into
	 * the source database oid directory while it is being deleted. We don't
	 * worry about aborts as we release session level locks automatically during
	 * an abort as opposed to a commit.
	 */
	if(Gp_role == GP_ROLE_DISPATCH)
		MoveDbSessionLockRelease();

	AtCommit_TablespaceStorage();

	AtEOXact_AppendOnly();
	AtCommit_Notify();
	AtEOXact_GUC(true, 1);
	AtEOXact_SPI(true);
	AtEOXact_on_commit_actions(true);
	AtEOXact_Namespace(true);
	AtEOXact_SMgr();
	AtEOXact_Files();
	AtEOXact_ComboCid();
	AtEOXact_HashTables(true);
	AtEOXact_PgStat(true);
	AtEOXact_Snapshot(true);
	pgstat_report_xact_timestamp(0);

	CurrentResourceOwner = NULL;
	ResourceOwnerDelete(TopTransactionResourceOwner);
	s->curTransactionOwner = NULL;
	CurTransactionResourceOwner = NULL;
	TopTransactionResourceOwner = NULL;

	AtCommit_Memory();

	finishDistributedTransactionContext("CommitTransaction", false);

	if (gp_local_distributed_cache_stats)
	{
		LocalDistribXactCache_ShowStats("CommitTransaction");
	}

	currentDistribXid = InvalidDistributedTransactionId;
	s->transactionId = InvalidTransactionId;
	s->subTransactionId = InvalidSubTransactionId;
	s->nestingLevel = 0;
	s->gucNestLevel = 0;
	s->childXids = NULL;
	s->nChildXids = 0;
	s->maxChildXids = 0;
	s->executorSaysXactDoesWrites = false;

	/*
	 * done with commit processing, set current transaction state back to
	 * default
	 */
	s->state = TRANS_DEFAULT;

	/* we're now in a consistent state to handle an interrupt. */
	RESUME_INTERRUPTS();

	/* Release resource group slot at the end of a transaction */
	if (ShouldUnassignResGroup())
		UnassignResGroup();
}

/*
 *	PrepareTransaction
 *
 * NB: if you change this routine, better look at CommitTransaction too!
 */
static void
PrepareTransaction(void)
{
	TransactionState s = CurrentTransactionState;
	TransactionId xid = GetCurrentTransactionId();
	GlobalTransaction gxact;
	TimestampTz prepared_at;

	ShowTransactionState("PrepareTransaction");

	/*
	 * check the current transaction state
	 */
	if (s->state != TRANS_INPROGRESS)
		elog(WARNING, "PrepareTransaction while in %s state",
			 TransStateAsString(s->state));
	Assert(s->parent == NULL);

	/*
	 * Do pre-commit processing that involves calling user-defined code, such
	 * as triggers.  Since closing cursors could queue trigger actions,
	 * triggers could open cursors, etc, we have to keep looping until there's
	 * nothing left to do.
	 */
	for (;;)
	{
		/*
		 * Fire all currently pending deferred triggers.
		 */
		AfterTriggerFireDeferred();

		/*
		 * Close open portals (converting holdable ones into static portals).
		 * If there weren't any, we are done ... otherwise loop back to check
		 * if they queued deferred triggers.  Lather, rinse, repeat.
		 */
		if (!PreCommit_Portals(true))
			break;
	}

	CallXactCallbacks(XACT_EVENT_PRE_PREPARE);

	/*
	 * The remaining actions cannot call any user-defined code, so it's safe
	 * to start shutting down within-transaction services.  But note that most
	 * of this stuff could still throw an error, which would switch us into
	 * the transaction-abort path.
	 */

	/* Shut down the deferred-trigger manager */
	AfterTriggerEndXact(true);

	AtEOXact_SharedSnapshot();

	/*
	 * Let ON COMMIT management do its thing (must happen after closing
	 * cursors, to avoid dangling-reference problems)
	 */
	PreCommit_on_commit_actions();

	AtEOXact_DispatchOids(true);

	/* close large objects before lower-level cleanup */
	AtEOXact_LargeObject(true);

	/*
	 * Mark serializable transaction as complete for predicate locking
	 * purposes.  This should be done as late as we can put it and still allow
	 * errors to be raised for failure patterns found at commit.
	 */
	PreCommit_CheckForSerializationFailure();

	/* NOTIFY will be handled below */

	/*
	 * In Postgres, MyXactAccessedTempRel is used to error out if PREPARE TRANSACTION
	 * operated on temp table.
	 *
	 * In GPDB, MyXactAccessedTempRel is removed.
	 *
	 * GPDB treat temporary table like a regular table, e.g. stored in shared buffer
	 * instead of keep it in local buffer. The temporary table just have a shorter life
	 * cycle either tie to the session or tie to the transaction if ON COMMIT clause is
	 * used.
	 *
	 * Every transaction in GPDB is 2PC, so PREPARE TRANSACTION is used even for temp table
	 * creation. GPDB cannot error out, otherwise, it won't be able to handle temp table
	 * at all.
	 */
#if 0 /* Upstream code not applicable to GPDB */
	/*
	 * Don't allow PREPARE TRANSACTION if we've accessed a temporary table in
	 * this transaction.  Having the prepared xact hold locks on another
	 * backend's temp table seems a bad idea --- for instance it would prevent
	 * the backend from exiting.  There are other problems too, such as how to
	 * clean up the source backend's local buffers and ON COMMIT state if the
	 * prepared xact includes a DROP of a temp table.
	 *
	 * We must check this after executing any ON COMMIT actions, because they
	 * might still access a temp relation.
	 *
	 * XXX In principle this could be relaxed to allow some useful special
	 * cases, such as a temp table created and dropped all within the
	 * transaction.  That seems to require much more bookkeeping though.
	 */
	if (MyXactAccessedTempRel)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot PREPARE a transaction that has operated on temporary tables")));
#endif
	SIMPLE_FAULT_INJECTOR("start_prepare");

	/*
	 * Likewise, don't allow PREPARE after pg_export_snapshot.  This could be
	 * supported if we added cleanup logic to twophase.c, but for now it
	 * doesn't seem worth the trouble.
	 */
	if (XactHasExportedSnapshots())
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		errmsg("cannot PREPARE a transaction that has exported snapshots")));

	/* Prevent cancel/die interrupt while cleaning up */
	HOLD_INTERRUPTS();

	/* Do some preparaty work on locks, before we change the transaction state. */
	PrePrepare_Locks();

	/*
	 * set the current transaction state information appropriately during
	 * prepare processing
	 */
	s->state = TRANS_PREPARE;

	prepared_at = GetCurrentTimestamp();

	/* Tell bufmgr and smgr to prepare for commit */
	BufmgrCommit();

	/*
	 * We cannot prepare if the xid is already aborted for some reason.
	 * If we proceed with this unexpected state, we'll be unrecoverable.
	 */
	if (TransactionIdDidAbort(xid))
		elog(ERROR, "xid %u is already aborted", xid);

	/*
	 * Reserve the GID for this transaction. This could fail if the requested
	 * GID is invalid or already in use.
	 */
	gxact = MarkAsPreparing(xid, &MyProc->localDistribXactData,
							prepareGID, prepared_at,
				GetUserId(), MyDatabaseId, InvalidXLogRecPtr);
	prepareGID = NULL;

	/*
	 * Collect data for the 2PC state file.  Note that in general, no actual
	 * state change should happen in the called modules during this step,
	 * since it's still possible to fail before commit, and in that case we
	 * want transaction abort to be able to clean up.  (In particular, the
	 * AtPrepare routines may error out if they find cases they cannot
	 * handle.)  State cleanup should happen in the PostPrepare routines
	 * below.  However, some modules can go ahead and clear state here because
	 * they wouldn't do anything with it during abort anyway.
	 *
	 * Note: because the 2PC state file records will be replayed in the same
	 * order they are made, the order of these calls has to match the order in
	 * which we want things to happen during COMMIT PREPARED or ROLLBACK
	 * PREPARED; in particular, pay attention to whether things should happen
	 * before or after releasing the transaction's locks.
	 */
	StartPrepare(gxact);

	AtPrepare_Notify();
	AtPrepare_Locks();
	AtPrepare_PredicateLocks();
	AtPrepare_PgStat();
	AtPrepare_MultiXact();
	AtPrepare_RelationMap();

	/*
	 * Here is where we really truly prepare.
	 *
	 * We have to record transaction prepares even if we didn't make any
	 * updates, because the transaction manager might get confused if we lose
	 * a global transaction.
	 */
	EndPrepare(gxact);

	/*
	 * Now we clean up backend-internal state and release internal resources.
	 */

	/* Reset XactLastRecEnd until the next transaction writes something */
	XactLastRecEnd = 0;

	/*
	 * Let others know about no transaction in progress by me.  This has to be
	 * done *after* the prepared transaction has been marked valid, else
	 * someone may think it is unlocked and recyclable.
	 */
	LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);
	ProcArrayClearTransaction(MyProc);
	LWLockRelease(ProcArrayLock);

	/*
	 * In normal commit-processing, this is all non-critical post-transaction
	 * cleanup.  When the transaction is prepared, however, it's important that
	 * the locks and other per-backend resources are transfered to the
	 * prepared transaction's PGPROC entry.  Note that if an error is raised
	 * here, it's too late to abort the transaction. XXX: This probably should
	 * be in a critical section, to force a PANIC if any of this fails, but
	 * that cure could be worse than the disease.
	 */

	CallXactCallbacks(XACT_EVENT_PREPARE);
	CallXactCallbacksOnce(XACT_EVENT_PREPARE);

	ResourceOwnerRelease(TopTransactionResourceOwner,
						 RESOURCE_RELEASE_BEFORE_LOCKS,
						 true, true);

	/* Check we've released all buffer pins */
	AtEOXact_Buffers(true);

	/* Clean up the relation cache */
	AtEOXact_RelationCache(true);

	/* notify doesn't need a postprepare call */

	PostPrepare_PgStat();

	PostPrepare_Inval();

	PostPrepare_smgr();

	PostPrepare_DatabaseStorage();

	PostPrepare_MultiXact(xid);

	PostPrepare_Locks(xid);
	PostPrepare_PredicateLocks(xid);

	ResourceOwnerRelease(TopTransactionResourceOwner,
						 RESOURCE_RELEASE_LOCKS,
						 true, true);
	ResourceOwnerRelease(TopTransactionResourceOwner,
						 RESOURCE_RELEASE_AFTER_LOCKS,
						 true, true);

	/*
	 * Allow another backend to finish the transaction.  After
	 * PostPrepare_Twophase(), the transaction is completely detached from
	 * our backend.  The rest is just non-critical cleanup of backend-local
	 * state.
	 */
	PostPrepare_Twophase();

	/* PREPARE acts the same as COMMIT as far as GUC is concerned */
	AtEOXact_GUC(true, 1);
	AtEOXact_SPI(true);
	AtEOXact_on_commit_actions(true);
	AtEOXact_Namespace(true);
	AtEOXact_SMgr();
	AtEOXact_Files();
	AtEOXact_ComboCid();
	AtEOXact_HashTables(true);
	/* don't call AtEOXact_PgStat here; we fixed pgstat state above */
	AtEOXact_Snapshot(true);
	pgstat_report_xact_timestamp(0);

	CurrentResourceOwner = NULL;
	ResourceOwnerDelete(TopTransactionResourceOwner);
	s->curTransactionOwner = NULL;
	CurTransactionResourceOwner = NULL;
	TopTransactionResourceOwner = NULL;

	AtCommit_Memory();

	if (gp_local_distributed_cache_stats)
	{
		LocalDistribXactCache_ShowStats("PrepareTransaction");
	}

	currentDistribXid = InvalidDistributedTransactionId;
	s->transactionId = InvalidTransactionId;
	s->subTransactionId = InvalidSubTransactionId;
	s->nestingLevel = 0;
	s->gucNestLevel = 0;
	s->childXids = NULL;
	s->nChildXids = 0;
	s->maxChildXids = 0;
	s->executorSaysXactDoesWrites = false;

	/*
	 * done with 1st phase commit processing, set current transaction state
	 * back to default
	 */
	s->state = TRANS_DEFAULT;

	RESUME_INTERRUPTS();

	/* Release resource group slot at the end of prepare transaction on segment */
	if (ShouldUnassignResGroup())
		UnassignResGroup();
}


/*
 *	AbortTransaction
 */
static void
AbortTransaction(void)
{
	TransactionState s = CurrentTransactionState;
	TransactionId latestXid;

	SIMPLE_FAULT_INJECTOR("transaction_abort_failure");

	/* Prevent cancel/die interrupt while cleaning up */
	HOLD_INTERRUPTS();

	/* Make sure we have a valid memory context and resource owner */
	AtAbort_Memory();
	AtAbort_ResourceOwner();

	/*
	 * Release any LW locks we might be holding as quickly as possible.
	 * (Regular locks, however, must be held till we finish aborting.)
	 * Releasing LW locks is critical since we might try to grab them again
	 * while cleaning up!
	 */
	LWLockReleaseAll();

	/* Clean up buffer I/O and buffer context locks, too */
	AbortBufferIO();
	UnlockBuffers();

	/*
	 * Also clean up any open wait for lock, since the lock manager will choke
	 * if we try to wait for another lock before doing this.
	 */
	LockErrorCleanup();

	/*
	 * If any timeout events are still active, make sure the timeout interrupt
	 * is scheduled.  This covers possible loss of a timeout interrupt due to
	 * longjmp'ing out of the SIGINT handler (see notes in handle_sig_alarm).
	 * We delay this till after LockErrorCleanup so that we don't uselessly
	 * reschedule lock or deadlock check timeouts.
	 */
	reschedule_timeouts();

	/*
	 * Re-enable signals, in case we got here by longjmp'ing out of a signal
	 * handler.  We do this fairly early in the sequence so that the timeout
	 * infrastructure will be functional if needed while aborting.
	 */
	PG_SETMASK(&UnBlockSig);

	/*
	 * check the current transaction state
	 */
	if (s->state != TRANS_INPROGRESS && s->state != TRANS_PREPARE)
		elog(DEBUG1, "WARNING: AbortTransaction while in %s state",
			 TransStateAsString(s->state));
	Assert(s->parent == NULL);

	/*
	 * set the current transaction state information appropriately during the
	 * abort processing
	 */
	s->state = TRANS_ABORT;

	/*
	 * Reset user ID which might have been changed transiently.  We need this
	 * to clean up in case control escaped out of a SECURITY DEFINER function
	 * or other local change of CurrentUserId; therefore, the prior value of
	 * SecurityRestrictionContext also needs to be restored.
	 *
	 * (Note: it is not necessary to restore session authorization or role
	 * settings here because those can only be changed via GUC, and GUC will
	 * take care of rolling them back if need be.)
	 */
	SetUserIdAndSecContext(s->prevUser, s->prevSecContext);

	/*
	 * do abort processing
	 */
	AfterTriggerEndXact(false); /* 'false' means it's abort */
	AtAbort_Portals();
	AtAbort_DispatcherState();
	AtEOXact_SharedSnapshot();

	/* Perform any Resource Scheduler abort procesing. */
	if (Gp_role == GP_ROLE_DISPATCH && IsResQueueEnabled())
		AtAbort_ResScheduler();
		
	/* Perform any AO table abort processing */
	AtAbort_AppendOnly();

	AtEOXact_DispatchOids(false);

	AtEOXact_LargeObject(false);
	AtAbort_Notify();
	AtEOXact_RelationMap(false);
	AtAbort_Twophase();

	/*
	 * Advertise the fact that we aborted in pg_clog (assuming that we got as
	 * far as assigning an XID to advertise).
	 */
	latestXid = RecordTransactionAbort(false);

	TRACE_POSTGRESQL_TRANSACTION_ABORT(MyProc->lxid);

	EndLocalDistribXact(false);
	/*
	 * Let others know about no transaction in progress by me. Note that this
	 * must be done _before_ releasing locks we hold and _after_
	 * RecordTransactionAbort.
	 */
	ProcArrayEndTransaction(MyProc, latestXid, false);

	/*
	 * Post-abort cleanup.  See notes in CommitTransaction() concerning
	 * ordering.  We can skip all of it if the transaction failed before
	 * creating a resource owner.
	 */
	if (TopTransactionResourceOwner != NULL)
	{
		CallXactCallbacks(XACT_EVENT_ABORT);
		CallXactCallbacksOnce(XACT_EVENT_ABORT);

		ResourceOwnerRelease(TopTransactionResourceOwner,
							 RESOURCE_RELEASE_BEFORE_LOCKS,
							 false, true);
		AtEOXact_Buffers(false);
		AtEOXact_RelationCache(false);
		AtEOXact_Inval(false);
		AtEOXact_MultiXact();

		ResourceOwnerRelease(TopTransactionResourceOwner,
							 RESOURCE_RELEASE_LOCKS,
							 false, true);
		ResourceOwnerRelease(TopTransactionResourceOwner,
							 RESOURCE_RELEASE_AFTER_LOCKS,
							 false, true);
		smgrDoPendingDeletes(false);

		DoPendingDbDeletes(false);
		DatabaseStorageResetSessionLock();

		AtAbort_TablespaceStorage();
		AtEOXact_AppendOnly();
		AtEOXact_GUC(false, 1);
		AtEOXact_SPI(false);
		AtEOXact_on_commit_actions(false);
		AtEOXact_Namespace(false);
		AtEOXact_SMgr();
		AtEOXact_Files();
		AtEOXact_ComboCid();
		AtEOXact_HashTables(false);
		AtEOXact_PgStat(false);
		pgstat_report_xact_timestamp(0);
	}

	/*
	 * Exported snapshots must be cleared before transaction ID is reset.  In
	 * GPDB, transaction ID is reset below.  In PostgreSQL, because 2PC is not
	 * needed, exported snapshots are cleared and transaction ID is reset
	 * later in CleanupTransaction().  We must perform both the actions here.
	 */
	AtEOXact_Snapshot(false);	/* and release the transaction's snapshots */

	/*
	 * Do abort to all QE. NOTE: we don't process
	 * signals to prevent recursion until we've notified the QEs.
	 *
	 * If something goes wrong after this, we might recurse back to
	 * AbortTransaction(). To avoid creating another Abort WAL record
	 * and failing assertion in ProcArrayEndTransaction because MyProc->xid
	 * has already been cleared, clear out transactionId now. The rest
	 * of the fields in TransactionState will be cleared later, in
	 * CleanupTransaction().
	 */
	TopTransactionStateData.transactionId = InvalidTransactionId;

	rollbackDtxTransaction();

	MyProc->localDistribXactData.state = LOCALDISTRIBXACT_STATE_NONE;

	/*
	 * State remains TRANS_ABORT until CleanupTransaction().
	 */
	RESUME_INTERRUPTS();

	/* If a query was cancelled, then cleanup reader gangs. */
	if (QueryCancelCleanup)
	{
		QueryCancelCleanup = false;
		cdbcomponent_cleanupIdleQEs(false);
	}

	/*
	 * If memprot decides to kill process, make sure we destroy all processes
	 * so that all mem/resource will be freed
	 */
	if (elog_geterrcode() == ERRCODE_GP_MEMPROT_KILL)
		DisconnectAndDestroyAllGangs(true);

	/* Release resource group slot at the end of a transaction */
	if (ShouldUnassignResGroup())
		UnassignResGroup();
}

/*
 *	CleanupTransaction
 */
static void
CleanupTransaction(void)
{
	TransactionState s = CurrentTransactionState;

	/*
	 * State should still be TRANS_ABORT from AbortTransaction().
	 */
	if (s->state != TRANS_ABORT)
		elog(FATAL, "CleanupTransaction: unexpected state %s",
			 TransStateAsString(s->state));

	/*
	 * do abort cleanup processing
	 */
	AtCleanup_Portals();		/* now safe to release portal memory */

	CurrentResourceOwner = NULL;	/* and resource owner */
	if (TopTransactionResourceOwner)
		ResourceOwnerDelete(TopTransactionResourceOwner);
	s->curTransactionOwner = NULL;
	CurTransactionResourceOwner = NULL;
	TopTransactionResourceOwner = NULL;

	AtCleanup_Memory();			/* and transaction memory */

	currentDistribXid = InvalidDistributedTransactionId;
	s->transactionId = InvalidTransactionId;
	s->subTransactionId = InvalidSubTransactionId;
	s->nestingLevel = 0;
	s->gucNestLevel = 0;
	s->childXids = NULL;
	s->nChildXids = 0;
	s->maxChildXids = 0;
	s->executorSaysXactDoesWrites = false;

	/*
	 * done with abort processing, set current transaction state back to
	 * default
	 */
	s->state = TRANS_DEFAULT;

	finishDistributedTransactionContext("CleanupTransaction", true);

	/* Release resource group slot at the end of a transaction */
	if (ShouldUnassignResGroup())
		UnassignResGroup();
}

/*
 *	StartTransactionCommand
 */
void
StartTransactionCommand(void)
{
	if (Gp_role == GP_ROLE_DISPATCH)
		setupRegularDtxContext();

	TransactionState s = CurrentTransactionState;

	switch (s->blockState)
	{
			/*
			 * if we aren't in a transaction block, we just do our usual start
			 * transaction.
			 */
		case TBLOCK_DEFAULT:
			StartTransaction();

			if (DistributedTransactionContext == DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER)
			{
				/*
				 * Pretend we executed an explicit BEGIN.
				 */
				s->blockState = TBLOCK_INPROGRESS;
			}
			else
			{
				/*
				 * Normal case.
				 */
				s->blockState = TBLOCK_STARTED;
			}
			break;

			/*
			 * We are somewhere in a transaction block or subtransaction and
			 * about to start a new command.  For now we do nothing, but
			 * someday we may do command-local resource initialization. (Note
			 * that any needed CommandCounterIncrement was done by the
			 * previous CommitTransactionCommand.)
			 */
		case TBLOCK_INPROGRESS:
		case TBLOCK_SUBINPROGRESS:
			/*
			 * There may be reader gangs waiting for us to update the
			 * QDSentXID -- make sure the state of the sharedsnapshot
			 * slot properly tracks the qd-xid
			 */
			if (Gp_role == GP_ROLE_EXECUTE && Gp_is_writer && SharedLocalSnapshotSlot != NULL)
			{
				LWLockAcquire(SharedLocalSnapshotSlot->slotLock, LW_EXCLUSIVE);

				TransactionId oldXid = SharedLocalSnapshotSlot->xid;
				TimestampTz oldStartTimestamp = SharedLocalSnapshotSlot->startTimestamp;

				/*
				 * MPP-3228: For a subtransaction, the transactionId
				 * may not have been assigned, we can't change the
				 * shared copy to InvalidTransactionId (the unassigned
				 * value) since the reader may *need* it).
				 */
				if (TransactionIdIsValid(s->transactionId))
				{
					SharedLocalSnapshotSlot->xid = s->transactionId;
				}

				SharedLocalSnapshotSlot->startTimestamp = xactStartTimestamp;
				SharedLocalSnapshotSlot->QDxid = QEDtxContextInfo.distributedXid;

				LWLockRelease(SharedLocalSnapshotSlot->slotLock);

				ereportif(Debug_print_full_dtm, LOG,
						  (errmsg("qExec WRITER updating shared xid: %u -> %u "
								  "(StartTransactionCommand) timestamp: "
								  INT64_FORMAT " -> " INT64_FORMAT ")",
								  oldXid, s->transactionId,
								  oldStartTimestamp, xactStartTimestamp)));
			}
			break;

			/*
			 * Here we are in a failed transaction block (one of the commands
			 * caused an abort) so we do nothing but remain in the abort
			 * state.  Eventually we will get a ROLLBACK command which will
			 * get us out of this state.  (It is up to other code to ensure
			 * that no commands other than ROLLBACK will be processed in these
			 * states.)
			 */
		case TBLOCK_ABORT:
		case TBLOCK_SUBABORT:
			break;

			/* These cases are invalid. */
		case TBLOCK_STARTED:
		case TBLOCK_BEGIN:
		case TBLOCK_SUBBEGIN:
		case TBLOCK_END:
		case TBLOCK_SUBRELEASE:
		case TBLOCK_SUBCOMMIT:
		case TBLOCK_ABORT_END:
		case TBLOCK_SUBABORT_END:
		case TBLOCK_ABORT_PENDING:
		case TBLOCK_SUBABORT_PENDING:
		case TBLOCK_SUBRESTART:
		case TBLOCK_SUBABORT_RESTART:
		case TBLOCK_PREPARE:
			elog(ERROR, "StartTransactionCommand: unexpected state %s",
				 BlockStateAsString(s->blockState));
			break;
	}

	/*
	 * We must switch to CurTransactionContext before returning. This is
	 * already done if we called StartTransaction, otherwise not.
	 */
	Assert(CurTransactionContext != NULL);
	MemoryContextSwitchTo(CurTransactionContext);
}

/*
 *	CommitTransactionCommand
 */
void
CommitTransactionCommand(void)
{
	TransactionState s = CurrentTransactionState;

	if (Gp_role == GP_ROLE_EXECUTE && !Gp_is_writer)
		elog(DEBUG1,"CommitTransactionCommand: called as segment Reader in state %s",
		     BlockStateAsString(s->blockState));

	switch (s->blockState)
	{
			/*
			 * This shouldn't happen, because it means the previous
			 * StartTransactionCommand didn't set the STARTED state
			 * appropriately.
			 */
		case TBLOCK_DEFAULT:
			elog(FATAL, "CommitTransactionCommand: unexpected state %s",
				 BlockStateAsString(s->blockState));
			break;

			/*
			 * If we aren't in a transaction block, just do our usual
			 * transaction commit, and return to the idle state.
			 */
		case TBLOCK_STARTED:
			CommitTransaction();
			s->blockState = TBLOCK_DEFAULT;
			break;

			/*
			 * We are completing a "BEGIN TRANSACTION" command, so we change
			 * to the "transaction block in progress" state and return.  (We
			 * assume the BEGIN did nothing to the database, so we need no
			 * CommandCounterIncrement.)
			 */
		case TBLOCK_BEGIN:
			s->blockState = TBLOCK_INPROGRESS;
			break;

			/*
			 * This is the case when we have finished executing a command
			 * someplace within a transaction block.  We increment the command
			 * counter and return.
			 */
		case TBLOCK_INPROGRESS:
		case TBLOCK_SUBINPROGRESS:
			CommandCounterIncrement();
			break;

			/*
			 * We are completing a "COMMIT" command.  Do it and return to the
			 * idle state.
			 */
		case TBLOCK_END:
			CommitTransaction();
			s->blockState = TBLOCK_DEFAULT;
			break;

			/*
			 * Here we are in the middle of a transaction block but one of the
			 * commands caused an abort so we do nothing but remain in the
			 * abort state.  Eventually we will get a ROLLBACK comand.
			 */
		case TBLOCK_ABORT:
		case TBLOCK_SUBABORT:
			break;

			/*
			 * Here we were in an aborted transaction block and we just got
			 * the ROLLBACK command from the user, so clean up the
			 * already-aborted transaction and return to the idle state.
			 */
		case TBLOCK_ABORT_END:
			CleanupTransaction();
			s->blockState = TBLOCK_DEFAULT;
			break;

			/*
			 * Here we were in a perfectly good transaction block but the user
			 * told us to ROLLBACK anyway.  We have to abort the transaction
			 * and then clean up.
			 */
		case TBLOCK_ABORT_PENDING:
			AbortTransaction();
			CleanupTransaction();
			s->blockState = TBLOCK_DEFAULT;
			break;

			/*
			 * We are completing a "PREPARE TRANSACTION" command.  Do it and
			 * return to the idle state.
			 */
		case TBLOCK_PREPARE:
			PrepareTransaction();
			s->blockState = TBLOCK_DEFAULT;
			break;

			/*
			 * We were just issued a SAVEPOINT inside a transaction block.
			 * Start a subtransaction.  (DefineSavepoint already did
			 * PushTransaction, so as to have someplace to put the SUBBEGIN
			 * state.)
			 */
		case TBLOCK_SUBBEGIN:
			StartSubTransaction();
			s->blockState = TBLOCK_SUBINPROGRESS;
			break;

			/*
			 * We were issued a RELEASE command, so we end the current
			 * subtransaction and return to the parent transaction. The parent
			 * might be ended too, so repeat till we find an INPROGRESS
			 * transaction or subtransaction.
			 */
		case TBLOCK_SUBRELEASE:
			do
			{
				CommitSubTransaction();
				s = CurrentTransactionState;	/* changed by pop */
			} while (s->blockState == TBLOCK_SUBRELEASE);

			Assert(s->blockState == TBLOCK_INPROGRESS ||
				   s->blockState == TBLOCK_SUBINPROGRESS);
			break;

			/*
			 * We were issued a COMMIT, so we end the current subtransaction
			 * hierarchy and perform final commit. We do this by rolling up
			 * any subtransactions into their parent, which leads to O(N^2)
			 * operations with respect to resource owners - this isn't that
			 * bad until we approach a thousands of savepoints but is
			 * necessary for correctness should after triggers create new
			 * resource owners.
			 */
		case TBLOCK_SUBCOMMIT:
			do
			{
				CommitSubTransaction();
				s = CurrentTransactionState;	/* changed by pop */
			} while (s->blockState == TBLOCK_SUBCOMMIT);
			/* If we had a COMMIT command, finish off the main xact too */
			if (s->blockState == TBLOCK_END)
			{
				Assert(s->parent == NULL);
				CommitTransaction();
				s->blockState = TBLOCK_DEFAULT;
			}
			else if (s->blockState == TBLOCK_PREPARE)
			{
				Assert(s->parent == NULL);
				PrepareTransaction();
				s->blockState = TBLOCK_DEFAULT;
			}
			else
				elog(ERROR, "CommitTransactionCommand: unexpected state %s",
					 BlockStateAsString(s->blockState));
			break;

			/*
			 * The current already-failed subtransaction is ending due to a
			 * ROLLBACK or ROLLBACK TO command, so pop it and recursively
			 * examine the parent (which could be in any of several states).
			 */
		case TBLOCK_SUBABORT_END:
			CleanupSubTransaction();
			CommitTransactionCommand();
			break;

			/*
			 * As above, but it's not dead yet, so abort first.
			 */
		case TBLOCK_SUBABORT_PENDING:
			AbortSubTransaction();
			CleanupSubTransaction();
			CommitTransactionCommand();
			break;

			/*
			 * The current subtransaction is the target of a ROLLBACK TO
			 * command.  Abort and pop it, then start a new subtransaction
			 * with the same name.
			 */
		case TBLOCK_SUBRESTART:
			{
				char	   *name;
				int			savepointLevel;

				/* save name and keep Cleanup from freeing it */
				name = s->name;
				s->name = NULL;
				savepointLevel = s->savepointLevel;

				AbortSubTransaction();
				CleanupSubTransaction();

				if (Gp_role == GP_ROLE_DISPATCH)
				{
					DispatchRollbackToSavepoint(name);
				}

				DefineSavepoint(name);
				s = CurrentTransactionState;	/* changed by push */
				if (name)
				{
					pfree(name);
				}
				s->savepointLevel = savepointLevel;

				/* This is the same as TBLOCK_SUBBEGIN case */
				AssertState(s->blockState == TBLOCK_SUBBEGIN);
				StartSubTransaction();
				s->blockState = TBLOCK_SUBINPROGRESS;
			}
			break;

			/*
			 * Same as above, but the subtransaction had already failed, so we
			 * don't need AbortSubTransaction.
			 */
		case TBLOCK_SUBABORT_RESTART:
			{
				char	   *name;
				int			savepointLevel;

				/* save name and keep Cleanup from freeing it */
				name = s->name;
				s->name = NULL;
				savepointLevel = s->savepointLevel;

				CleanupSubTransaction();

				if (Gp_role == GP_ROLE_DISPATCH)
				{
					DispatchRollbackToSavepoint(name);
				}

				DefineSavepoint(name);
				s = CurrentTransactionState;	/* changed by push */
				s->name = name;
				s->savepointLevel = savepointLevel;

				/* This is the same as TBLOCK_SUBBEGIN case */
				AssertState(s->blockState == TBLOCK_SUBBEGIN);
				StartSubTransaction();
				s->blockState = TBLOCK_SUBINPROGRESS;
			}
			break;
	}
}

/*
 *	AbortCurrentTransaction
 */
void
AbortCurrentTransaction(void)
{
	TransactionState s = CurrentTransactionState;
	
	elog(DEBUG5, "AbortCurrentTransaction for %d in state: %d", 
		s->transactionId, 
		s->blockState);

	switch (s->blockState)
	{
		case TBLOCK_DEFAULT:
			if (s->state == TRANS_DEFAULT)
			{
				/* we are idle, so nothing to do */
			}
			else
			{
				/*
				 * We can get here after an error during transaction start
				 * (state will be TRANS_START).  Need to clean up the
				 * incompletely started transaction.  First, adjust the
				 * low-level state to suppress warning message from
				 * AbortTransaction.
				 */
				if (s->state == TRANS_START)
					s->state = TRANS_INPROGRESS;
				AbortTransaction();
				CleanupTransaction();
			}
			break;

			/*
			 * if we aren't in a transaction block, we just do the basic abort
			 * & cleanup transaction.
			 */
		case TBLOCK_STARTED:
			AbortTransaction();
			CleanupTransaction();
			s->blockState = TBLOCK_DEFAULT;
			break;

			/*
			 * If we are in TBLOCK_BEGIN it means something screwed up right
			 * after reading "BEGIN TRANSACTION".  We assume that the user
			 * will interpret the error as meaning the BEGIN failed to get him
			 * into a transaction block, so we should abort and return to idle
			 * state.
			 */
		case TBLOCK_BEGIN:
			AbortTransaction();
			CleanupTransaction();
			s->blockState = TBLOCK_DEFAULT;
			break;

			/*
			 * We are somewhere in a transaction block and we've gotten a
			 * failure, so we abort the transaction and set up the persistent
			 * ABORT state.  We will stay in ABORT until we get a ROLLBACK.
			 */
		case TBLOCK_INPROGRESS:
			AbortTransaction();
			s->blockState = TBLOCK_ABORT;
			/* CleanupTransaction happens when we exit TBLOCK_ABORT_END */
			break;

			/*
			 * Here, we failed while trying to COMMIT.  Clean up the
			 * transaction and return to idle state (we do not want to stay in
			 * the transaction).
			 */
		case TBLOCK_END:
			AbortTransaction();
			CleanupTransaction();
			s->blockState = TBLOCK_DEFAULT;
			break;

			/*
			 * Here, we are already in an aborted transaction state and are
			 * waiting for a ROLLBACK, but for some reason we failed again! So
			 * we just remain in the abort state.
			 */
		case TBLOCK_ABORT:
		case TBLOCK_SUBABORT:
			break;

			/*
			 * We are in a failed transaction and we got the ROLLBACK command.
			 * We have already aborted, we just need to cleanup and go to idle
			 * state.
			 */
		case TBLOCK_ABORT_END:
			CleanupTransaction();
			s->blockState = TBLOCK_DEFAULT;
			
			Assert(DistributedTransactionContext == DTX_CONTEXT_LOCAL_ONLY);
			break;

			/*
			 * We are in a live transaction and we got a ROLLBACK command.
			 * Abort, cleanup, go to idle state.
			 */
		case TBLOCK_ABORT_PENDING:
			AbortTransaction();
			CleanupTransaction();
			s->blockState = TBLOCK_DEFAULT;
			
			Assert(DistributedTransactionContext == DTX_CONTEXT_LOCAL_ONLY);
			break;

			/*
			 * Here, we failed while trying to PREPARE.  Clean up the
			 * transaction and return to idle state (we do not want to stay in
			 * the transaction).
			 */
		case TBLOCK_PREPARE:
			AbortTransaction();
			CleanupTransaction();
			s->blockState = TBLOCK_DEFAULT;
			
			Assert(DistributedTransactionContext == DTX_CONTEXT_LOCAL_ONLY);
			break;

			/*
			 * We got an error inside a subtransaction.  Abort just the
			 * subtransaction, and go to the persistent SUBABORT state until
			 * we get ROLLBACK.
			 */
		case TBLOCK_SUBINPROGRESS:
			AbortSubTransaction();
			s->blockState = TBLOCK_SUBABORT;
			break;

			/*
			 * If we failed while trying to create a subtransaction, clean up
			 * the broken subtransaction and abort the parent.  The same
			 * applies if we get a failure while ending a subtransaction.
			 */
		case TBLOCK_SUBBEGIN:
		case TBLOCK_SUBRELEASE:
		case TBLOCK_SUBCOMMIT:
		case TBLOCK_SUBABORT_PENDING:
		case TBLOCK_SUBRESTART:
			AbortSubTransaction();
			CleanupSubTransaction();
			AbortCurrentTransaction();
			break;

			/*
			 * Same as above, except the Abort() was already done.
			 */
		case TBLOCK_SUBABORT_END:
		case TBLOCK_SUBABORT_RESTART:
			CleanupSubTransaction();
			AbortCurrentTransaction();
			break;
	}
}

/*
 *	PreventTransactionChain
 *
 *	This routine is to be called by statements that must not run inside
 *	a transaction block, typically because they have non-rollback-able
 *	side effects or do internal commits.
 *
 *	If we have already started a transaction block, issue an error; also issue
 *	an error if we appear to be running inside a user-defined function (which
 *	could issue more commands and possibly cause a failure after the statement
 *	completes).  Subtransactions are verboten too.
 *
 *	isTopLevel: passed down from ProcessUtility to determine whether we are
 *	inside a function or multi-query querystring.  (We will always fail if
 *	this is false, but it's convenient to centralize the check here instead of
 *	making callers do it.)
 *	stmtType: statement type name, for error messages.
 */
void
PreventTransactionChain(bool isTopLevel, const char *stmtType)
{
	/*
	 * xact block already started?
	 */
	if (IsTransactionBlock())
		ereport(ERROR,
				(errcode(ERRCODE_ACTIVE_SQL_TRANSACTION),
		/* translator: %s represents an SQL statement name */
				 errmsg("%s cannot run inside a transaction block",
						stmtType)));

	/*
	 * subtransaction?
	 */
	if (IsSubTransaction())
		ereport(ERROR,
				(errcode(ERRCODE_ACTIVE_SQL_TRANSACTION),
		/* translator: %s represents an SQL statement name */
				 errmsg("%s cannot run inside a subtransaction",
						stmtType)));

	/*
	 * inside a function call?
	 */
	if (!isTopLevel)
		ereport(ERROR,
				(errcode(ERRCODE_ACTIVE_SQL_TRANSACTION),
		/* translator: %s represents an SQL statement name */
				 errmsg("%s cannot be executed from a function or multi-command string",
						stmtType)));

	/* If we got past IsTransactionBlock test, should be in default state */
	if (CurrentTransactionState->blockState != TBLOCK_DEFAULT &&
		CurrentTransactionState->blockState != TBLOCK_STARTED)
		elog(FATAL, "cannot prevent transaction chain");
	/* all okay */
}

/*
 *	These two functions allow for warnings or errors if a command is
 *	executed outside of a transaction block.
 *
 *	While top-level transaction control commands (BEGIN/COMMIT/ABORT) and
 *	SET that have no effect issue warnings, all other no-effect commands
 *	generate errors.
 */
void
WarnNoTransactionChain(bool isTopLevel, const char *stmtType)
{
	CheckTransactionChain(isTopLevel, false, stmtType);
}

void
RequireTransactionChain(bool isTopLevel, const char *stmtType)
{
	CheckTransactionChain(isTopLevel, true, stmtType);
}

/*
 *	RequireTransactionChain
 *
 *	This routine is to be called by statements that must run inside
 *	a transaction block, because they have no effects that persist past
 *	transaction end (and so calling them outside a transaction block
 *	is presumably an error).  DECLARE CURSOR is an example.
 *
 *	If we appear to be running inside a user-defined function, we do not
 *	issue anything, since the function could issue more commands that make
 *	use of the current statement's results.  Likewise subtransactions.
 *	Thus this is an inverse for PreventTransactionChain.
 *
 *	isTopLevel: passed down from ProcessUtility to determine whether we are
 *	inside a function.
 *	stmtType: statement type name, for warning or error messages.
 */
static void
CheckTransactionChain(bool isTopLevel, bool throwError, const char *stmtType)
{
	/*
	 * xact block already started?
	 */
	if (IsTransactionBlock())
		return;

	/*
	 * subtransaction?
	 */
	if (IsSubTransaction())
		return;

	/*
	 * inside a function call?
	 */
	if (!isTopLevel)
		return;

	ereport(throwError ? ERROR : WARNING,
			(errcode(ERRCODE_NO_ACTIVE_SQL_TRANSACTION),
	/* translator: %s represents an SQL statement name */
			 errmsg("%s can only be used in transaction blocks",
					stmtType)));
	return;
}

/*
 *	IsInTransactionChain
 *
 *	This routine is for statements that need to behave differently inside
 *	a transaction block than when running as single commands.  ANALYZE is
 *	currently the only example.
 *
 *	isTopLevel: passed down from ProcessUtility to determine whether we are
 *	inside a function.
 */
bool
IsInTransactionChain(bool isTopLevel)
{
	/*
	 * Return true on same conditions that would make PreventTransactionChain
	 * error out
	 */
	if (IsTransactionBlock())
		return true;

	if (IsSubTransaction())
		return true;

	if (!isTopLevel)
		return true;

	if (CurrentTransactionState->blockState != TBLOCK_DEFAULT &&
		CurrentTransactionState->blockState != TBLOCK_STARTED)
		return true;

	return false;
}


/*
 * Register or deregister callback functions for start- and end-of-xact
 * operations.
 *
 * These functions are intended for use by dynamically loaded modules.
 * For built-in modules we generally just hardwire the appropriate calls
 * (mainly because it's easier to control the order that way, where needed).
 *
 * At transaction end, the callback occurs post-commit or post-abort, so the
 * callback functions can only do noncritical cleanup.
 */
void
RegisterXactCallback(XactCallback callback, void *arg)
{
	XactCallbackItem *item;

	item = (XactCallbackItem *)
		MemoryContextAlloc(TopMemoryContext, sizeof(XactCallbackItem));
	item->callback = callback;
	item->arg = arg;
	item->next = Xact_callbacks;
	Xact_callbacks = item;
}

void
UnregisterXactCallback(XactCallback callback, void *arg)
{
	XactCallbackItem *item;
	XactCallbackItem *prev;

	prev = NULL;
	for (item = Xact_callbacks; item; prev = item, item = item->next)
	{
		if (item->callback == callback && item->arg == arg)
		{
			if (prev)
				prev->next = item->next;
			else
				Xact_callbacks = item->next;
			pfree(item);
			break;
		}
	}
}

static void
CallXactCallbacks(XactEvent event)
{
	XactCallbackItem *item;

	for (item = Xact_callbacks; item; item = item->next)
		(*item->callback) (event, item->arg);
}

/* Register or deregister callback functions for start/end Xact.  Call only once. */
void
RegisterXactCallbackOnce(XactCallback callback, void *arg)
{
	XactCallbackItem *item;

	item = (XactCallbackItem *)
		MemoryContextAlloc(TopMemoryContext, sizeof(XactCallbackItem));
	item->callback = callback;
	item->arg = arg;
	item->next = Xact_callbacks_once;
	Xact_callbacks_once = item;
}

void
UnregisterXactCallbackOnce(XactCallback callback, void *arg)
{
	XactCallbackItem *item;
	XactCallbackItem *prev;

	prev = NULL;
	for (item = Xact_callbacks_once; item; prev = item, item = item->next)
	{
		if (item->callback == callback && item->arg == arg)
		{
			if (prev)
				prev->next = item->next;
			else
				Xact_callbacks_once = item->next;
			pfree(item);
			break;
		}
	}
}

static void
CallXactCallbacksOnce(XactEvent event)
{
	/* currently callback once should ignore prepare. */
	if (event == XACT_EVENT_PREPARE)
		return;

	while(Xact_callbacks_once)
	{
		XactCallbackItem *next = Xact_callbacks_once->next;
		XactCallback callback=Xact_callbacks_once->callback;
		void*arg=Xact_callbacks_once->arg;
		pfree(Xact_callbacks_once);
		Xact_callbacks_once = next;
		callback(event,arg);
	}
}

/*
 * Register or deregister callback functions for start- and end-of-subxact
 * operations.
 *
 * Pretty much same as above, but for subtransaction events.
 *
 * At subtransaction end, the callback occurs post-subcommit or post-subabort,
 * so the callback functions can only do noncritical cleanup.  At
 * subtransaction start, the callback is called when the subtransaction has
 * finished initializing.
 */
void
RegisterSubXactCallback(SubXactCallback callback, void *arg)
{
	SubXactCallbackItem *item;

	item = (SubXactCallbackItem *)
		MemoryContextAlloc(TopMemoryContext, sizeof(SubXactCallbackItem));
	item->callback = callback;
	item->arg = arg;
	item->next = SubXact_callbacks;
	SubXact_callbacks = item;
}

void
UnregisterSubXactCallback(SubXactCallback callback, void *arg)
{
	SubXactCallbackItem *item;
	SubXactCallbackItem *prev;

	prev = NULL;
	for (item = SubXact_callbacks; item; prev = item, item = item->next)
	{
		if (item->callback == callback && item->arg == arg)
		{
			if (prev)
				prev->next = item->next;
			else
				SubXact_callbacks = item->next;
			pfree(item);
			break;
		}
	}
}

static void
CallSubXactCallbacks(SubXactEvent event,
					 SubTransactionId mySubid,
					 SubTransactionId parentSubid)
{
	SubXactCallbackItem *item;

	for (item = SubXact_callbacks; item; item = item->next)
		(*item->callback) (event, mySubid, parentSubid, item->arg);
}


/* ----------------------------------------------------------------
 *					   transaction block support
 * ----------------------------------------------------------------
 */

/*
 *	BeginTransactionBlock
 *		This executes a BEGIN command.
 */
void
BeginTransactionBlock(void)
{
	TransactionState s = CurrentTransactionState;

	switch (s->blockState)
	{
			/*
			 * We are not inside a transaction block, so allow one to begin.
			 */
		case TBLOCK_STARTED:
			s->blockState = TBLOCK_BEGIN;
			break;

			/*
			 * Already a transaction block in progress.
			 */
		case TBLOCK_INPROGRESS:
		case TBLOCK_SUBINPROGRESS:
		case TBLOCK_ABORT:
		case TBLOCK_SUBABORT:
			if (Gp_role == GP_ROLE_EXECUTE)
				ereport(DEBUG1,
						(errcode(ERRCODE_ACTIVE_SQL_TRANSACTION),
						 errmsg("there is already a transaction in progress")));
			else
				ereport(WARNING,
						(errcode(ERRCODE_ACTIVE_SQL_TRANSACTION),
						 errmsg("there is already a transaction in progress")));
			break;

			/* These cases are invalid. */
		case TBLOCK_DEFAULT:
		case TBLOCK_BEGIN:
		case TBLOCK_SUBBEGIN:
		case TBLOCK_END:
		case TBLOCK_SUBRELEASE:
		case TBLOCK_SUBCOMMIT:
		case TBLOCK_ABORT_END:
		case TBLOCK_SUBABORT_END:
		case TBLOCK_ABORT_PENDING:
		case TBLOCK_SUBABORT_PENDING:
		case TBLOCK_SUBRESTART:
		case TBLOCK_SUBABORT_RESTART:
		case TBLOCK_PREPARE:
			elog(FATAL, "BeginTransactionBlock: unexpected state %s",
				 BlockStateAsString(s->blockState));
			break;
	}
}

/*
 *	PrepareTransactionBlock
 *		This executes a PREPARE command.
 *
 * Since PREPARE may actually do a ROLLBACK, the result indicates what
 * happened: TRUE for PREPARE, FALSE for ROLLBACK.
 *
 * Note that we don't actually do anything here except change blockState.
 * The real work will be done in the upcoming PrepareTransaction().
 * We do it this way because it's not convenient to change memory context,
 * resource owner, etc while executing inside a Portal.
 */
bool
PrepareTransactionBlock(char *gid)
{
	TransactionState s;
	bool		result;

	/* Set up to commit the current transaction */
	result = EndTransactionBlock();

	/* If successful, change outer tblock state to PREPARE */
	if (result)
	{
		s = CurrentTransactionState;

		while (s->parent != NULL)
			s = s->parent;

		if (s->blockState == TBLOCK_END)
		{
			/* Save GID where PrepareTransaction can find it again */
			prepareGID = MemoryContextStrdup(TopTransactionContext, gid);

			s->blockState = TBLOCK_PREPARE;
		}
		else
		{
			/*
			 * ignore case where we are not in a transaction;
			 * EndTransactionBlock already issued a warning.
			 */
			Assert(s->blockState == TBLOCK_STARTED);
			/* Don't send back a PREPARE result tag... */
			result = false;
		}
	}

	return result;
}

/*
 *	EndTransactionBlock
 *		This executes a COMMIT command.
 *
 * Since COMMIT may actually do a ROLLBACK, the result indicates what
 * happened: TRUE for COMMIT, FALSE for ROLLBACK.
 *
 * Note that we don't actually do anything here except change blockState.
 * The real work will be done in the upcoming CommitTransactionCommand().
 * We do it this way because it's not convenient to change memory context,
 * resource owner, etc while executing inside a Portal.
 */
bool
EndTransactionBlock(void)
{
	TransactionState s = CurrentTransactionState;
	bool		result = false;

	switch (s->blockState)
	{
			/*
			 * We are in a transaction block, so tell CommitTransactionCommand
			 * to COMMIT.
			 */
		case TBLOCK_INPROGRESS:
			s->blockState = TBLOCK_END;
			result = true;
			break;

			/*
			 * We are in a failed transaction block.  Tell
			 * CommitTransactionCommand it's time to exit the block.
			 */
		case TBLOCK_ABORT:
			s->blockState = TBLOCK_ABORT_END;
			break;

			/*
			 * We are in a live subtransaction block.  Set up to subcommit all
			 * open subtransactions and then commit the main transaction.
			 */
		case TBLOCK_SUBINPROGRESS:
			while (s->parent != NULL)
			{
				if (s->blockState == TBLOCK_SUBINPROGRESS)
					s->blockState = TBLOCK_SUBCOMMIT;
				else
					elog(FATAL, "EndTransactionBlock: unexpected state %s",
						 BlockStateAsString(s->blockState));
				s = s->parent;
			}
			if (s->blockState == TBLOCK_INPROGRESS)
				s->blockState = TBLOCK_END;
			else
				elog(FATAL, "EndTransactionBlock: unexpected state %s",
					 BlockStateAsString(s->blockState));
			result = true;
			break;

			/*
			 * Here we are inside an aborted subtransaction.  Treat the COMMIT
			 * as ROLLBACK: set up to abort everything and exit the main
			 * transaction.
			 */
		case TBLOCK_SUBABORT:
			while (s->parent != NULL)
			{
				if (s->blockState == TBLOCK_SUBINPROGRESS)
					s->blockState = TBLOCK_SUBABORT_PENDING;
				else if (s->blockState == TBLOCK_SUBABORT)
					s->blockState = TBLOCK_SUBABORT_END;
				else
					elog(FATAL, "EndTransactionBlock: unexpected state %s",
						 BlockStateAsString(s->blockState));
				s = s->parent;
			}
			if (s->blockState == TBLOCK_INPROGRESS)
				s->blockState = TBLOCK_ABORT_PENDING;
			else if (s->blockState == TBLOCK_ABORT)
				s->blockState = TBLOCK_ABORT_END;
			else
				elog(FATAL, "EndTransactionBlock: unexpected state %s",
					 BlockStateAsString(s->blockState));
			break;

			/*
			 * The user issued COMMIT when not inside a transaction.  Issue a
			 * WARNING, staying in TBLOCK_STARTED state.  The upcoming call to
			 * CommitTransactionCommand() will then close the transaction and
			 * put us back into the default state.
			 */
		case TBLOCK_STARTED:
			if (Gp_role == GP_ROLE_EXECUTE)
			{
				ereport(DEBUG2,
						(errcode(ERRCODE_NO_ACTIVE_SQL_TRANSACTION),
						 errmsg("there is no transaction in progress")));
			}
			else
				ereport(WARNING,
						(errcode(ERRCODE_NO_ACTIVE_SQL_TRANSACTION),
						 errmsg("there is no transaction in progress")));
			result = true;
			break;

			/* These cases are invalid. */
		case TBLOCK_DEFAULT:
		case TBLOCK_BEGIN:
		case TBLOCK_SUBBEGIN:
		case TBLOCK_END:
		case TBLOCK_SUBRELEASE:
		case TBLOCK_SUBCOMMIT:
		case TBLOCK_ABORT_END:
		case TBLOCK_SUBABORT_END:
		case TBLOCK_ABORT_PENDING:
		case TBLOCK_SUBABORT_PENDING:
		case TBLOCK_SUBRESTART:
		case TBLOCK_SUBABORT_RESTART:
		case TBLOCK_PREPARE:
			elog(FATAL, "EndTransactionBlock: unexpected state %s",
				 BlockStateAsString(s->blockState));
			break;
	}

	return result;
}

/*
 *	UserAbortTransactionBlock
 *		This executes a ROLLBACK command.
 *
 * As above, we don't actually do anything here except change blockState.
 */
void
UserAbortTransactionBlock(void)
{
	TransactionState s = CurrentTransactionState;

	switch (s->blockState)
	{
			/*
			 * We are inside a transaction block and we got a ROLLBACK command
			 * from the user, so tell CommitTransactionCommand to abort and
			 * exit the transaction block.
			 */
		case TBLOCK_INPROGRESS:
			s->blockState = TBLOCK_ABORT_PENDING;
			break;

			/*
			 * We are inside a failed transaction block and we got a ROLLBACK
			 * command from the user.  Abort processing is already done, so
			 * CommitTransactionCommand just has to cleanup and go back to
			 * idle state.
			 */
		case TBLOCK_ABORT:
			s->blockState = TBLOCK_ABORT_END;
			break;

			/*
			 * We are inside a subtransaction.  Mark everything up to top
			 * level as exitable.
			 */
		case TBLOCK_SUBINPROGRESS:
		case TBLOCK_SUBABORT:
			while (s->parent != NULL)
			{
				if (s->blockState == TBLOCK_SUBINPROGRESS)
					s->blockState = TBLOCK_SUBABORT_PENDING;
				else if (s->blockState == TBLOCK_SUBABORT)
					s->blockState = TBLOCK_SUBABORT_END;
				else
					elog(FATAL, "UserAbortTransactionBlock: unexpected state %s",
						 BlockStateAsString(s->blockState));
				s = s->parent;
			}
			if (s->blockState == TBLOCK_INPROGRESS)
				s->blockState = TBLOCK_ABORT_PENDING;
			else if (s->blockState == TBLOCK_ABORT)
				s->blockState = TBLOCK_ABORT_END;
			else
				elog(FATAL, "UserAbortTransactionBlock: unexpected state %s",
					 BlockStateAsString(s->blockState));
			break;

			/*
			 * The user issued ABORT when not inside a transaction. Issue a
			 * WARNING and go to abort state.  The upcoming call to
			 * CommitTransactionCommand() will then put us back into the
			 * default state.
			 */
		case TBLOCK_STARTED:
			ereport((Gp_role == GP_ROLE_EXECUTE) ? DEBUG2 : WARNING,
					(errcode(ERRCODE_NO_ACTIVE_SQL_TRANSACTION),
					 errmsg("there is no transaction in progress")));
			s->blockState = TBLOCK_ABORT_PENDING;
			break;

			/* These cases are invalid. */
		case TBLOCK_DEFAULT:
		case TBLOCK_BEGIN:
		case TBLOCK_SUBBEGIN:
		case TBLOCK_END:
		case TBLOCK_SUBRELEASE:
		case TBLOCK_SUBCOMMIT:
		case TBLOCK_ABORT_END:
		case TBLOCK_SUBABORT_END:
		case TBLOCK_ABORT_PENDING:
		case TBLOCK_SUBABORT_PENDING:
		case TBLOCK_SUBRESTART:
		case TBLOCK_SUBABORT_RESTART:
		case TBLOCK_PREPARE:
			elog(FATAL, "UserAbortTransactionBlock: unexpected state %s",
				 BlockStateAsString(s->blockState));
			break;
	}
}

void
DefineDispatchSavepoint(char *name)
{
	TransactionState s = CurrentTransactionState;

	if ((s->blockState != TBLOCK_INPROGRESS) &&
	    (s->blockState != TBLOCK_SUBINPROGRESS))
	{
		elog(FATAL, "DefineSavepoint: unexpected state %s",
			    BlockStateAsString(s->blockState));
	}	

	/* First we attempt to create on the QEs */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		char	   *cmd;

		cmd = psprintf("SAVEPOINT %s", quote_identifier(name));

		/*
		 * dispatch a DTX command, in the event of an error, this call
		 * will either exit via elog()/ereport() or return false
		 */
		if (!dispatchDtxCommand(cmd))
			elog(ERROR, "Could not create a new savepoint (%s)", cmd);

		pfree(cmd);
	}

	DefineSavepoint(name);
}

/*
 * DefineSavepoint
 *		This executes a SAVEPOINT command.
 */
void
DefineSavepoint(char *name)
{
	TransactionState s = CurrentTransactionState;

	switch (s->blockState)
	{
		case TBLOCK_INPROGRESS:
		case TBLOCK_SUBINPROGRESS:
			/* Normal subtransaction start */
			PushTransaction();
			s = CurrentTransactionState;		/* changed by push */

			/*
			 * Savepoint names, like the TransactionState block itself, live
			 * in TopTransactionContext.
			 */
			if (name)
				s->name = MemoryContextStrdup(TopTransactionContext, name);
			break;

			/* These cases are invalid. */
		case TBLOCK_DEFAULT:
		case TBLOCK_STARTED:
		case TBLOCK_BEGIN:
		case TBLOCK_SUBBEGIN:
		case TBLOCK_END:
		case TBLOCK_SUBRELEASE:
		case TBLOCK_SUBCOMMIT:
		case TBLOCK_ABORT:
		case TBLOCK_SUBABORT:
		case TBLOCK_ABORT_END:
		case TBLOCK_SUBABORT_END:
		case TBLOCK_ABORT_PENDING:
		case TBLOCK_SUBABORT_PENDING:
		case TBLOCK_SUBRESTART:
		case TBLOCK_SUBABORT_RESTART:
		case TBLOCK_PREPARE:
			elog(FATAL, "DefineSavepoint: unexpected state %s",
				 BlockStateAsString(s->blockState));
			break;
	}
}

/*
 * ReleaseSavepoint
 *		This executes a RELEASE command.
 *
 * As above, we don't actually do anything here except change blockState.
 */
void
ReleaseSavepoint(List *options)
{
	TransactionState s = CurrentTransactionState;
	TransactionState target,
				xact;
	ListCell   *cell;
	char	   *name = NULL;

	switch (s->blockState)
	{
			/*
			 * We can't rollback to a savepoint if there is no savepoint
			 * defined.
			 */
		case TBLOCK_INPROGRESS:
			ereport(ERROR,
					(errcode(ERRCODE_S_E_INVALID_SPECIFICATION),
					 errmsg("no such savepoint")));
			break;

			/*
			 * We are in a non-aborted subtransaction.  This is the only valid
			 * case.
			 */
		case TBLOCK_SUBINPROGRESS:
			break;

			/* These cases are invalid. */
		case TBLOCK_DEFAULT:
		case TBLOCK_STARTED:
		case TBLOCK_BEGIN:
		case TBLOCK_SUBBEGIN:
		case TBLOCK_END:
		case TBLOCK_SUBRELEASE:
		case TBLOCK_SUBCOMMIT:
		case TBLOCK_ABORT:
		case TBLOCK_SUBABORT:
		case TBLOCK_ABORT_END:
		case TBLOCK_SUBABORT_END:
		case TBLOCK_ABORT_PENDING:
		case TBLOCK_SUBABORT_PENDING:
		case TBLOCK_SUBRESTART:
		case TBLOCK_SUBABORT_RESTART:
		case TBLOCK_PREPARE:
			elog(FATAL, "ReleaseSavepoint: unexpected state %s",
				 BlockStateAsString(s->blockState));
			break;
	}

	foreach(cell, options)
	{
		DefElem    *elem = lfirst(cell);

		if (strcmp(elem->defname, "savepoint_name") == 0)
			name = strVal(elem->arg);
	}

	Assert(PointerIsValid(name));

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		char	   *cmd;

		cmd = psprintf("RELEASE SAVEPOINT %s", quote_identifier(name));

		/*
		 * dispatch a DTX command, in the event of an error, this call will
		 * either exit via elog()/ereport() or return false
		 */
		if (!dispatchDtxCommand(cmd))
			elog(ERROR, "Could not release savepoint (%s)", cmd);

		pfree(cmd);
	}

	for (target = s; PointerIsValid(target); target = target->parent)
	{
		if (PointerIsValid(target->name) && strcmp(target->name, name) == 0)
			break;
	}

	if (!PointerIsValid(target))
		ereport(ERROR,
				(errcode(ERRCODE_S_E_INVALID_SPECIFICATION),
				 errmsg("no such savepoint")));

	/* disallow crossing savepoint level boundaries */
	if (target->savepointLevel != s->savepointLevel)
		ereport(ERROR,
				(errcode(ERRCODE_S_E_INVALID_SPECIFICATION),
				 errmsg("no such savepoint")));

	/*
	 * Mark "commit pending" all subtransactions up to the target
	 * subtransaction.  The actual commits will happen when control gets to
	 * CommitTransactionCommand.
	 */
	xact = CurrentTransactionState;
	for (;;)
	{
		Assert(xact->blockState == TBLOCK_SUBINPROGRESS);
		xact->blockState = TBLOCK_SUBRELEASE;
		if (xact == target)
			break;
		xact = xact->parent;
		Assert(PointerIsValid(xact));
	}
}

/*
 * RollbackToSavepoint
 *		This executes a ROLLBACK TO <savepoint> command.
 *
 * As above, we don't actually do anything here except change blockState.
 */
void
RollbackToSavepoint(List *options)
{
	TransactionState s = CurrentTransactionState;
	TransactionState target,
				xact;
	ListCell   *cell;
	char	   *name = NULL;

	switch (s->blockState)
	{
			/*
			 * We can't rollback to a savepoint if there is no savepoint
			 * defined.
			 */
		case TBLOCK_INPROGRESS:
		case TBLOCK_ABORT:
			ereport(ERROR,
					(errcode(ERRCODE_S_E_INVALID_SPECIFICATION),
					 errmsg("no such savepoint")));
			break;

			/*
			 * There is at least one savepoint, so proceed.
			 */
		case TBLOCK_SUBINPROGRESS:
		case TBLOCK_SUBABORT:
			break;

			/* These cases are invalid. */
		case TBLOCK_DEFAULT:
		case TBLOCK_STARTED:
		case TBLOCK_BEGIN:
		case TBLOCK_SUBBEGIN:
		case TBLOCK_END:
		case TBLOCK_SUBRELEASE:
		case TBLOCK_SUBCOMMIT:
		case TBLOCK_ABORT_END:
		case TBLOCK_SUBABORT_END:
		case TBLOCK_ABORT_PENDING:
		case TBLOCK_SUBABORT_PENDING:
		case TBLOCK_SUBRESTART:
		case TBLOCK_SUBABORT_RESTART:
		case TBLOCK_PREPARE:
			elog(FATAL, "RollbackToSavepoint: unexpected state %s",
				 BlockStateAsString(s->blockState));
			break;
	}

	foreach(cell, options)
	{
		DefElem    *elem = lfirst(cell);

		if (strcmp(elem->defname, "savepoint_name") == 0)
			name = strVal(elem->arg);
	}

	Assert(PointerIsValid(name));

	for (target = s; PointerIsValid(target); target = target->parent)
	{
		if (PointerIsValid(target->name) && strcmp(target->name, name) == 0)
			break;
	}

	if (!PointerIsValid(target))
		ereport(ERROR,
				(errcode(ERRCODE_S_E_INVALID_SPECIFICATION),
				 errmsg("no such savepoint")));

	/* disallow crossing savepoint level boundaries */
	if (target->savepointLevel != s->savepointLevel)
		ereport(ERROR,
				(errcode(ERRCODE_S_E_INVALID_SPECIFICATION),
				 errmsg("no such savepoint")));

	/*
	 * Mark "abort pending" all subtransactions up to the target
	 * subtransaction.  The actual aborts will happen when control gets to
	 * CommitTransactionCommand.
	 */
	xact = CurrentTransactionState;
	for (;;)
	{
		if (xact == target)
			break;
		if (xact->blockState == TBLOCK_SUBINPROGRESS)
			xact->blockState = TBLOCK_SUBABORT_PENDING;
		else if (xact->blockState == TBLOCK_SUBABORT)
			xact->blockState = TBLOCK_SUBABORT_END;
		else
			elog(FATAL, "RollbackToSavepoint: unexpected state %s",
				 BlockStateAsString(xact->blockState));
		xact = xact->parent;
		Assert(PointerIsValid(xact));
	}

	/* And mark the target as "restart pending" */
	if (xact->blockState == TBLOCK_SUBINPROGRESS)
		xact->blockState = TBLOCK_SUBRESTART;
	else if (xact->blockState == TBLOCK_SUBABORT)
		xact->blockState = TBLOCK_SUBABORT_RESTART;
	else
		elog(FATAL, "RollbackToSavepoint: unexpected state %s",
			 BlockStateAsString(xact->blockState));
}

static void
DispatchRollbackToSavepoint(char *name)
{
	char	   *cmd;

	if (!name)
		elog(ERROR, "could not find savepoint name for ROLLBACK TO SAVEPOINT");

	cmd = psprintf("ROLLBACK TO SAVEPOINT %s", quote_identifier(name));

	/*
	 * dispatch a DTX command, in the event of an error, this call will
	 * either exit via elog()/ereport() or return false
	 */
	if (!dispatchDtxCommand(cmd))
		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("Could not rollback to savepoint (%s)", cmd)));

	pfree(cmd);
}

/*
 * BeginInternalSubTransaction
 *		This is the same as DefineSavepoint except it allows TBLOCK_STARTED,
 *		TBLOCK_END, and TBLOCK_PREPARE states, and therefore it can safely be
 *		used in functions that might be called when not inside a BEGIN block
 *		or when running deferred triggers at COMMIT/PREPARE time.  Also, it
 *		automatically does CommitTransactionCommand/StartTransactionCommand
 *		instead of expecting the caller to do it.
 */
void
BeginInternalSubTransaction(char *name)
{
	TransactionState s = CurrentTransactionState;

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		if (!doDispatchSubtransactionInternalCmd(
			DTX_PROTOCOL_COMMAND_SUBTRANSACTION_BEGIN_INTERNAL))
		{
			elog(ERROR, 
				"Could not BeginInternalSubTransaction dispatch failed");
		}
	}

	switch (s->blockState)
	{
		case TBLOCK_STARTED:
		case TBLOCK_INPROGRESS:
		case TBLOCK_END:
		case TBLOCK_PREPARE:
		case TBLOCK_SUBINPROGRESS:
			/* Normal subtransaction start */
			PushTransaction();
			s = CurrentTransactionState;		/* changed by push */

			/*
			 * Savepoint names, like the TransactionState block itself, live
			 * in TopTransactionContext.
			 */
			if (name)
				s->name = MemoryContextStrdup(TopTransactionContext, name);
			break;

			/* These cases are invalid. */
		case TBLOCK_DEFAULT:
		case TBLOCK_BEGIN:
		case TBLOCK_SUBBEGIN:
		case TBLOCK_SUBRELEASE:
		case TBLOCK_SUBCOMMIT:
		case TBLOCK_ABORT:
		case TBLOCK_SUBABORT:
		case TBLOCK_ABORT_END:
		case TBLOCK_SUBABORT_END:
		case TBLOCK_ABORT_PENDING:
		case TBLOCK_SUBABORT_PENDING:
		case TBLOCK_SUBRESTART:
		case TBLOCK_SUBABORT_RESTART:
			elog(FATAL, "BeginInternalSubTransaction: unexpected state %s",
				 BlockStateAsString(s->blockState));
			break;
	}

	CommitTransactionCommand();
	StartTransactionCommand();
}

/*
 * ReleaseCurrentSubTransaction
 *
 * RELEASE (ie, commit) the innermost subtransaction, regardless of its
 * savepoint name (if any).
 * NB: do NOT use CommitTransactionCommand/StartTransactionCommand with this.
 */
void
ReleaseCurrentSubTransaction(void)
{
	TransactionState s = CurrentTransactionState;

	if (s->blockState != TBLOCK_SUBINPROGRESS)
		elog(ERROR, "ReleaseCurrentSubTransaction: unexpected state %s",
			 BlockStateAsString(s->blockState));
	Assert(s->state == TRANS_INPROGRESS);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		if (!doDispatchSubtransactionInternalCmd(
			DTX_PROTOCOL_COMMAND_SUBTRANSACTION_RELEASE_INTERNAL))
		{
			elog(ERROR, 
				"Could not ReleaseCurrentSubTransaction dispatch failed");
		}
	}

	MemoryContextSwitchTo(CurTransactionContext);
	CommitSubTransaction();
	s = CurrentTransactionState;	/* changed by pop */
	Assert(s->state == TRANS_INPROGRESS);
}

/*
 * RollbackAndReleaseCurrentSubTransaction
 *
 * ROLLBACK and RELEASE (ie, abort) the innermost subtransaction, regardless
 * of its savepoint name (if any).
 * NB: do NOT use CommitTransactionCommand/StartTransactionCommand with this.
 */
void
RollbackAndReleaseCurrentSubTransaction(void)
{
	TransactionState s = CurrentTransactionState;

	switch (s->blockState)
	{
			/* Must be in a subtransaction */
		case TBLOCK_SUBINPROGRESS:
		case TBLOCK_SUBABORT:
			break;

			/* These cases are invalid. */
		case TBLOCK_DEFAULT:
		case TBLOCK_STARTED:
		case TBLOCK_BEGIN:
		case TBLOCK_SUBBEGIN:
		case TBLOCK_INPROGRESS:
		case TBLOCK_END:
		case TBLOCK_SUBRELEASE:
		case TBLOCK_SUBCOMMIT:
		case TBLOCK_ABORT:
		case TBLOCK_ABORT_END:
		case TBLOCK_SUBABORT_END:
		case TBLOCK_ABORT_PENDING:
		case TBLOCK_SUBABORT_PENDING:
		case TBLOCK_SUBRESTART:
		case TBLOCK_SUBABORT_RESTART:
		case TBLOCK_PREPARE:
			elog(FATAL, "RollbackAndReleaseCurrentSubTransaction: unexpected state %s",
				 BlockStateAsString(s->blockState));
			break;
	}

	/*
	 * Abort the current subtransaction, if needed.
	 */
	if (s->blockState == TBLOCK_SUBINPROGRESS)
		AbortSubTransaction();

	/* And clean it up, too */
	CleanupSubTransaction();

	s = CurrentTransactionState;	/* changed by pop */
	AssertState(s->blockState == TBLOCK_SUBINPROGRESS ||
				s->blockState == TBLOCK_INPROGRESS ||
				s->blockState == TBLOCK_STARTED);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		if (!doDispatchSubtransactionInternalCmd(
				DTX_PROTOCOL_COMMAND_SUBTRANSACTION_ROLLBACK_INTERNAL))
		{
			ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
							errmsg("DTX RollbackAndReleaseCurrentSubTransaction dispatch failed")));
		}
	}
}

void
CommitNotPreparedTransaction(void)
{
	TransactionState s = CurrentTransactionState;

	while (s->blockState == TBLOCK_SUBINPROGRESS)
	{
		CommitSubTransaction();
		s = CurrentTransactionState;
	}

	if (s->blockState == TBLOCK_INPROGRESS)
	{
		Assert(s->parent == NULL);
		CommitTransaction();
	}
	s->blockState = TBLOCK_DEFAULT;
	MyProc->localDistribXactData.state = LOCALDISTRIBXACT_STATE_NONE;
	return;
}
/*
 *	AbortOutOfAnyTransaction
 *
 *	This routine is provided for error recovery purposes.  It aborts any
 *	active transaction or transaction block, leaving the system in a known
 *	idle state.
 */
void
AbortOutOfAnyTransaction(void)
{
	TransactionState s = CurrentTransactionState;

	/* Ensure we're not running in a doomed memory context */
	AtAbort_Memory();

	/*
	 * Get out of any transaction or nested transaction
	 */
	do
	{
		switch (s->blockState)
		{
			case TBLOCK_DEFAULT:
				if (s->state == TRANS_DEFAULT)
				{
					/* Not in a transaction, do nothing */
				}
				else
				{
					/*
					 * We can get here after an error during transaction start
					 * (state will be TRANS_START).  Need to clean up the
					 * incompletely started transaction.  First, adjust the
					 * low-level state to suppress warning message from
					 * AbortTransaction.
					 */
					if (s->state == TRANS_START)
						s->state = TRANS_INPROGRESS;
					AbortTransaction();
					CleanupTransaction();
				}
				break;
			case TBLOCK_STARTED:
			case TBLOCK_BEGIN:
			case TBLOCK_INPROGRESS:
			case TBLOCK_END:
			case TBLOCK_ABORT_PENDING:
			case TBLOCK_PREPARE:
				/* In a transaction, so clean up */
				AbortTransaction();
				CleanupTransaction();
				s->blockState = TBLOCK_DEFAULT;

				Assert(DistributedTransactionContext == DTX_CONTEXT_LOCAL_ONLY);
				break;
			case TBLOCK_ABORT:
			case TBLOCK_ABORT_END:

				/*
				 * AbortTransaction is already done, still need Cleanup.
				 * However, if we failed partway through running ROLLBACK,
				 * there will be an active portal running that command, which
				 * we need to shut down before doing CleanupTransaction.
				 */
				AtAbort_Portals();
				CleanupTransaction();
				s->blockState = TBLOCK_DEFAULT;

				Assert(DistributedTransactionContext == DTX_CONTEXT_LOCAL_ONLY);
				break;

				/*
				 * In a subtransaction, so clean it up and abort parent too
				 */
			case TBLOCK_SUBBEGIN:
			case TBLOCK_SUBINPROGRESS:
			case TBLOCK_SUBRELEASE:
			case TBLOCK_SUBCOMMIT:
			case TBLOCK_SUBABORT_PENDING:
			case TBLOCK_SUBRESTART:
				AbortSubTransaction();
				CleanupSubTransaction();
				s = CurrentTransactionState;	/* changed by pop */
				break;

			case TBLOCK_SUBABORT:
			case TBLOCK_SUBABORT_END:
			case TBLOCK_SUBABORT_RESTART:
				/* As above, but AbortSubTransaction already done */
				if (s->curTransactionOwner)
				{
					/* As in TBLOCK_ABORT, might have a live portal to zap */
					AtSubAbort_Portals(s->subTransactionId,
									   s->parent->subTransactionId,
									   s->curTransactionOwner,
									   s->parent->curTransactionOwner);
				}
				CleanupSubTransaction();
				s = CurrentTransactionState;	/* changed by pop */
				break;
		}
	} while (s->blockState != TBLOCK_DEFAULT);

	/* Should be out of all subxacts now */
	Assert(s->parent == NULL);

	/* If we didn't actually have anything to do, revert to TopMemoryContext */
	AtCleanup_Memory();
}

/*
 * IsTransactionBlock --- are we within a transaction block?
 */
bool
IsTransactionBlock(void)
{
	TransactionState s = CurrentTransactionState;

	if (s->blockState == TBLOCK_DEFAULT || s->blockState == TBLOCK_STARTED)
		return false;

	return true;
}

/*
 * IsTransactionOrTransactionBlock --- are we within either a transaction
 * or a transaction block?	(The backend is only really "idle" when this
 * returns false.)
 *
 * This should match up with IsTransactionBlock and IsTransactionState.
 */
bool
IsTransactionOrTransactionBlock(void)
{
	TransactionState s = CurrentTransactionState;

	if (s->blockState == TBLOCK_DEFAULT)
		return false;

	return true;
}

void
ExecutorMarkTransactionUsesSequences(void)
{
	seqXlogWrite = true;

	ForceSyncCommit();
}

void 
ExecutorMarkTransactionDoesWrites(void)
{
	// UNDONE: Verify we are in transaction...
	if (!TopTransactionStateData.executorSaysXactDoesWrites)
	{
		ereportif(Debug_print_full_dtm, LOG,
				  (errmsg("ExecutorMarkTransactionDoesWrites called")));
		TopTransactionStateData.executorSaysXactDoesWrites = true;
	}
}

bool
ExecutorSaysTransactionDoesWrites(void)
{
	return TopTransactionStateData.executorSaysXactDoesWrites;
}

/*
 * TransactionBlockStatusCode - return status code to send in ReadyForQuery
 */
char
TransactionBlockStatusCode(void)
{
	TransactionState s = CurrentTransactionState;

	switch (s->blockState)
	{
		case TBLOCK_DEFAULT:
		case TBLOCK_STARTED:
			return 'I';			/* idle --- not in transaction */
		case TBLOCK_BEGIN:
		case TBLOCK_SUBBEGIN:
		case TBLOCK_INPROGRESS:
		case TBLOCK_SUBINPROGRESS:
		case TBLOCK_END:
		case TBLOCK_SUBRELEASE:
		case TBLOCK_SUBCOMMIT:
		case TBLOCK_PREPARE:
			return 'T';			/* in transaction */
		case TBLOCK_ABORT:
		case TBLOCK_SUBABORT:
		case TBLOCK_ABORT_END:
		case TBLOCK_SUBABORT_END:
		case TBLOCK_ABORT_PENDING:
		case TBLOCK_SUBABORT_PENDING:
		case TBLOCK_SUBRESTART:
		case TBLOCK_SUBABORT_RESTART:
			return 'E';			/* in failed transaction */
	}

	/* should never get here */
	elog(FATAL, "invalid transaction block state: %s",
		 BlockStateAsString(s->blockState));
	return 0;					/* keep compiler quiet */
}

/*
 * IsSubTransaction
 */
bool
IsSubTransaction(void)
{
	TransactionState s = CurrentTransactionState;

	if (s->nestingLevel >= 2)
		return true;

	return false;
}

/*
 * StartSubTransaction
 *
 * If you're wondering why this is separate from PushTransaction: it's because
 * we can't conveniently do this stuff right inside DefineSavepoint.  The
 * SAVEPOINT utility command will be executed inside a Portal, and if we
 * muck with CurrentMemoryContext or CurrentResourceOwner then exit from
 * the Portal will undo those settings.  So we make DefineSavepoint just
 * push a dummy transaction block, and when control returns to the main
 * idle loop, CommitTransactionCommand will be called, and we'll come here
 * to finish starting the subtransaction.
 */
static void
StartSubTransaction(void)
{
	TransactionState s = CurrentTransactionState;

	if (s->state != TRANS_DEFAULT)
		elog(WARNING, "StartSubTransaction while in %s state",
			 TransStateAsString(s->state));

	s->state = TRANS_START;

	/*
	 * Initialize subsystems for new subtransaction
	 *
	 * must initialize resource-management stuff first
	 */
	AtSubStart_Memory();
	AtSubStart_ResourceOwner();
	AtSubStart_Inval();
	AtSubStart_Notify();
	AfterTriggerBeginSubXact();

	s->state = TRANS_INPROGRESS;

	/*
	 * Call start-of-subxact callbacks
	 */
	CallSubXactCallbacks(SUBXACT_EVENT_START_SUB, s->subTransactionId,
						 s->parent->subTransactionId);

	ShowTransactionState("StartSubTransaction");
}

/*
 * CommitSubTransaction
 *
 *	The caller has to make sure to always reassign CurrentTransactionState
 *	if it has a local pointer to it after calling this function.
 */
static void
CommitSubTransaction(void)
{
	TransactionState s = CurrentTransactionState;

	ShowTransactionState("CommitSubTransaction");

	if (s->state != TRANS_INPROGRESS)
		elog(WARNING, "CommitSubTransaction while in %s state",
			 TransStateAsString(s->state));

	/* Pre-commit processing goes here */

	CallSubXactCallbacks(SUBXACT_EVENT_PRE_COMMIT_SUB, s->subTransactionId,
						 s->parent->subTransactionId);

	/* Do the actual "commit", such as it is */
	s->state = TRANS_COMMIT;

	/* Must CCI to ensure commands of subtransaction are seen as done */
	CommandCounterIncrement();

	/*
	 * Prior to 8.4 we marked subcommit in clog at this point.  We now only
	 * perform that step, if required, as part of the atomic update of the
	 * whole transaction tree at top level commit or abort.
	 */

	/* Post-commit cleanup */
	if (TransactionIdIsValid(s->transactionId))
		AtSubCommit_childXids();
	AfterTriggerEndSubXact(true);
	AtSubCommit_Portals(s->subTransactionId,
						s->parent->subTransactionId,
						s->parent->curTransactionOwner);
	AtEOSubXact_LargeObject(true, s->subTransactionId,
							s->parent->subTransactionId);
	AtSubCommit_Notify();

	CallSubXactCallbacks(SUBXACT_EVENT_COMMIT_SUB, s->subTransactionId,
						 s->parent->subTransactionId);

	ResourceOwnerRelease(s->curTransactionOwner,
						 RESOURCE_RELEASE_BEFORE_LOCKS,
						 true, false);
	AtEOSubXact_RelationCache(true, s->subTransactionId,
							  s->parent->subTransactionId);
	AtEOSubXact_Inval(true);
	AtSubCommit_smgr();

	/*
	 * The only lock we actually release here is the subtransaction XID lock.
	 */
	CurrentResourceOwner = s->curTransactionOwner;
	if (TransactionIdIsValid(s->transactionId))
		XactLockTableDelete(s->transactionId);

	/*
	 * Other locks should get transferred to their parent resource owner.
	 */
	ResourceOwnerRelease(s->curTransactionOwner,
						 RESOURCE_RELEASE_LOCKS,
						 true, false);
	ResourceOwnerRelease(s->curTransactionOwner,
						 RESOURCE_RELEASE_AFTER_LOCKS,
						 true, false);

	AtEOXact_GUC(true, s->gucNestLevel);
	AtEOSubXact_SPI(true, s->subTransactionId);
	AtEOSubXact_on_commit_actions(true, s->subTransactionId,
								  s->parent->subTransactionId);
	AtEOSubXact_Namespace(true, s->subTransactionId,
						  s->parent->subTransactionId);
	AtEOSubXact_Files(true, s->subTransactionId,
					  s->parent->subTransactionId);
	AtEOSubXact_HashTables(true, s->nestingLevel);
	AtEOSubXact_PgStat(true, s->nestingLevel);
	AtSubCommit_Snapshot(s->nestingLevel);

	/*
	 * We need to restore the upper transaction's read-only state, in case the
	 * upper is read-write while the child is read-only; GUC will incorrectly
	 * think it should leave the child state in place.
	 */
	XactReadOnly = s->prevXactReadOnly;

	CurrentResourceOwner = s->parent->curTransactionOwner;
	CurTransactionResourceOwner = s->parent->curTransactionOwner;
	ResourceOwnerDelete(s->curTransactionOwner);
	s->curTransactionOwner = NULL;

	AtSubCommit_Memory();

	s->state = TRANS_DEFAULT;

	PopTransaction();
}

/*
 * AbortSubTransaction
 */
static void
AbortSubTransaction(void)
{
	TransactionState s = CurrentTransactionState;

	/* Prevent cancel/die interrupt while cleaning up */
	HOLD_INTERRUPTS();

	/* Make sure we have a valid memory context and resource owner */
	AtSubAbort_Memory();
	AtSubAbort_ResourceOwner();

	/*
	 * Release any LW locks we might be holding as quickly as possible.
	 * (Regular locks, however, must be held till we finish aborting.)
	 * Releasing LW locks is critical since we might try to grab them again
	 * while cleaning up!
	 *
	 * FIXME This may be incorrect --- Are there some locks we should keep?
	 * Buffer locks, for example?  I don't think so but I'm not sure.
	 */
	LWLockReleaseAll();

	AbortBufferIO();
	UnlockBuffers();

	/*
	 * Also clean up any open wait for lock, since the lock manager will choke
	 * if we try to wait for another lock before doing this.
	 */
	LockErrorCleanup();

	/*
	 * If any timeout events are still active, make sure the timeout interrupt
	 * is scheduled.  This covers possible loss of a timeout interrupt due to
	 * longjmp'ing out of the SIGINT handler (see notes in handle_sig_alarm).
	 * We delay this till after LockErrorCleanup so that we don't uselessly
	 * reschedule lock or deadlock check timeouts.
	 */
	reschedule_timeouts();

	/*
	 * Re-enable signals, in case we got here by longjmp'ing out of a signal
	 * handler.  We do this fairly early in the sequence so that the timeout
	 * infrastructure will be functional if needed while aborting.
	 */
	PG_SETMASK(&UnBlockSig);

	/*
	 * check the current transaction state
	 */
	ShowTransactionState("AbortSubTransaction");

	if (s->state != TRANS_INPROGRESS)
		elog(WARNING, "AbortSubTransaction while in %s state",
			 TransStateAsString(s->state));

	s->state = TRANS_ABORT;

	/*
	 * Reset user ID which might have been changed transiently.  (See notes in
	 * AbortTransaction.)
	 */
	SetUserIdAndSecContext(s->prevUser, s->prevSecContext);

	/*
	 * We can skip all this stuff if the subxact failed before creating a
	 * ResourceOwner...
	 */
	if (s->curTransactionOwner)
	{
		AfterTriggerEndSubXact(false);
		AtSubAbort_Portals(s->subTransactionId,
						   s->parent->subTransactionId,
						   s->curTransactionOwner,
						   s->parent->curTransactionOwner);
		AtSubAbort_DispatcherState();
		AtEOXact_DispatchOids(false);
		AtEOSubXact_LargeObject(false, s->subTransactionId,
								s->parent->subTransactionId);
		AtSubAbort_Notify();

		/* Advertise the fact that we aborted in pg_clog. */
		(void) RecordTransactionAbort(true);

		/* Post-abort cleanup */
		if (TransactionIdIsValid(s->transactionId))
			AtSubAbort_childXids();

		CallSubXactCallbacks(SUBXACT_EVENT_ABORT_SUB, s->subTransactionId,
							 s->parent->subTransactionId);

		ResourceOwnerRelease(s->curTransactionOwner,
							 RESOURCE_RELEASE_BEFORE_LOCKS,
							 false, false);
		AtEOSubXact_RelationCache(false, s->subTransactionId,
								  s->parent->subTransactionId);
		AtEOSubXact_Inval(false);
		ResourceOwnerRelease(s->curTransactionOwner,
							 RESOURCE_RELEASE_LOCKS,
							 false, false);
		ResourceOwnerRelease(s->curTransactionOwner,
							 RESOURCE_RELEASE_AFTER_LOCKS,
							 false, false);
		AtSubAbort_smgr();

		AtEOXact_GUC(false, s->gucNestLevel);
		AtEOSubXact_SPI(false, s->subTransactionId);
		AtEOSubXact_on_commit_actions(false, s->subTransactionId,
									  s->parent->subTransactionId);
		AtEOSubXact_Namespace(false, s->subTransactionId,
							  s->parent->subTransactionId);
		AtEOSubXact_Files(false, s->subTransactionId,
						  s->parent->subTransactionId);
		AtEOSubXact_HashTables(false, s->nestingLevel);
		AtEOSubXact_PgStat(false, s->nestingLevel);
		AtSubAbort_Snapshot(s->nestingLevel);
	}

	/*
	 * Restore the upper transaction's read-only state, too.  This should be
	 * redundant with GUC's cleanup but we may as well do it for consistency
	 * with the commit case.
	 */
	XactReadOnly = s->prevXactReadOnly;

	RESUME_INTERRUPTS();
}

/*
 * CleanupSubTransaction
 *
 *	The caller has to make sure to always reassign CurrentTransactionState
 *	if it has a local pointer to it after calling this function.
 */
static void
CleanupSubTransaction(void)
{
	TransactionState s = CurrentTransactionState;

	ShowTransactionState("CleanupSubTransaction");

	if (s->state != TRANS_ABORT)
		elog(WARNING, "CleanupSubTransaction while in %s state",
			 TransStateAsString(s->state));

	AtSubCleanup_Portals(s->subTransactionId);

	CurrentResourceOwner = s->parent->curTransactionOwner;
	CurTransactionResourceOwner = s->parent->curTransactionOwner;
	if (s->curTransactionOwner)
		ResourceOwnerDelete(s->curTransactionOwner);
	s->curTransactionOwner = NULL;

	AtSubCleanup_Memory();

	s->state = TRANS_DEFAULT;

	PopTransaction();
}

/*
 * PushTransaction
 *		Create transaction state stack entry for a subtransaction
 *
 *	The caller has to make sure to always reassign CurrentTransactionState
 *	if it has a local pointer to it after calling this function.
 */
static void
PushTransaction(void)
{
	TransactionState p = CurrentTransactionState;
	TransactionState s;

	currentSavepointTotal++;

	if ((currentSavepointTotal >= gp_subtrans_warn_limit) &&
	    (currentSavepointTotal % gp_subtrans_warn_limit == 0))
	{
		ereport(WARNING,
		(errmsg("Using too many subtransactions in one transaction."),
		errhint("Close open transactions soon to avoid wraparound "
			"problems.")));
	}

	/*
	 * We keep subtransaction state nodes in TopTransactionContext.
	 */
	s = (TransactionState)
		MemoryContextAllocZero(TopTransactionContext,
							   sizeof(TransactionStateData));

	/*
	 * Assign a subtransaction ID, watching out for counter wraparound.
	 */
	currentSubTransactionId += 1;
	if (currentSubTransactionId == InvalidSubTransactionId)
	{
		currentSubTransactionId -= 1;
		pfree(s);
		ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("cannot have more than 2^32-1 subtransactions in a transaction")));
	}

	/*
	 * We can now stack a minimally valid subtransaction without fear of
	 * failure.
	 */
	s->transactionId = InvalidTransactionId;	/* until assigned */
	s->subTransactionId = currentSubTransactionId;
	s->parent = p;
	s->nestingLevel = p->nestingLevel + 1;
	s->gucNestLevel = NewGUCNestLevel();
	s->savepointLevel = p->savepointLevel;
	s->state = TRANS_DEFAULT;
	s->blockState = TBLOCK_SUBBEGIN;
	GetUserIdAndSecContext(&s->prevUser, &s->prevSecContext);
	s->prevXactReadOnly = XactReadOnly;
	s->executorSaysXactDoesWrites = false;

	fastNodeCount++;
	if (fastNodeCount == NUM_NODES_TO_SKIP_FOR_FAST_SEARCH)
	{
		fastNodeCount = 0;
		s->fastLink = previousFastLink;
		previousFastLink = s;
	}

	CurrentTransactionState = s;

	/*
	 * AbortSubTransaction and CleanupSubTransaction have to be able to cope
	 * with the subtransaction from here on out; in particular they should not
	 * assume that it necessarily has a transaction context, resource owner,
	 * or XID.
	 */
}

/*
 * PopTransaction
 *		Pop back to parent transaction state
 *
 *	The caller has to make sure to always reassign CurrentTransactionState
 *	if it has a local pointer to it after calling this function.
 */
static void
PopTransaction(void)
{
	TransactionState s = CurrentTransactionState;

	if (s->state != TRANS_DEFAULT)
		elog(WARNING, "PopTransaction while in %s state",
			 TransStateAsString(s->state));

	if (s->parent == NULL)
		elog(FATAL, "PopTransaction with no parent");

	CurrentTransactionState = s->parent;

	/* Let's just make sure CurTransactionContext is good */
	CurTransactionContext = s->parent->curTransactionContext;
	MemoryContextSwitchTo(CurTransactionContext);

	/* Ditto for ResourceOwner links */
	CurTransactionResourceOwner = s->parent->curTransactionOwner;
	CurrentResourceOwner = s->parent->curTransactionOwner;

	if (fastNodeCount)
	{
		fastNodeCount--;
	}

	/*
	 * Deleting node where last fastLink is stored
	 * hence retrive the fastLink to update in node to be added next
	 */
	if (previousFastLink == s)
	{
		fastNodeCount = NUM_NODES_TO_SKIP_FOR_FAST_SEARCH - 1;
		previousFastLink = s->fastLink;
	}

	/* Free the old child structure */
	if (s->name)
		pfree(s->name);
	pfree(s);
}

/*
 * ShowTransactionState
 *		Debug support
 */
static void
ShowTransactionState(const char *str)
{
	/* skip work if message will definitely not be printed */
	if (log_min_messages <= DEBUG3 || client_min_messages <= DEBUG3)
	{
		elog(DEBUG3, "%s", str);
		ShowTransactionStateRec(CurrentTransactionState);
	}
}

/*
 * ShowTransactionStateRec
 *		Recursive subroutine for ShowTransactionState
 */
static void
ShowTransactionStateRec(TransactionState s)
{
	StringInfoData buf;

	initStringInfo(&buf);

	if (s->nChildXids > 0)
	{
		int			i;

		appendStringInfo(&buf, "%u", s->childXids[0]);
		for (i = 1; i < s->nChildXids; i++)
			appendStringInfo(&buf, " %u", s->childXids[i]);
	}

	if (s->parent)
		ShowTransactionStateRec(s->parent);

	/* use ereport to suppress computation if msg will not be printed */
	ereport(DEBUG3,
			(errmsg_internal("name: %s; blockState: %13s; state: %7s, xid/subid/cid: %u/%u/%u%s, nestlvl: %d, children: %s",
							 PointerIsValid(s->name) ? s->name : "unnamed",
							 BlockStateAsString(s->blockState),
							 TransStateAsString(s->state),
							 (unsigned int) s->transactionId,
							 (unsigned int) s->subTransactionId,
							 (unsigned int) currentCommandId,
							 currentCommandIdUsed ? " (used)" : "",
							 s->nestingLevel, buf.data)));

	pfree(buf.data);
}

/*
 * BlockStateAsString
 *		Debug support
 */
static const char *
BlockStateAsString(TBlockState blockState)
{
	switch (blockState)
	{
		case TBLOCK_DEFAULT:
			return "DEFAULT";
		case TBLOCK_STARTED:
			return "STARTED";
		case TBLOCK_BEGIN:
			return "BEGIN";
		case TBLOCK_INPROGRESS:
			return "INPROGRESS";
		case TBLOCK_END:
			return "END";
		case TBLOCK_ABORT:
			return "ABORT";
		case TBLOCK_ABORT_END:
			return "ABORT END";
		case TBLOCK_ABORT_PENDING:
			return "ABORT PEND";
		case TBLOCK_PREPARE:
			return "PREPARE";
		case TBLOCK_SUBBEGIN:
			return "SUB BEGIN";
		case TBLOCK_SUBINPROGRESS:
			return "SUB INPROGRS";
		case TBLOCK_SUBRELEASE:
			return "SUB RELEASE";
		case TBLOCK_SUBCOMMIT:
			return "SUB COMMIT";
		case TBLOCK_SUBABORT:
			return "SUB ABORT";
		case TBLOCK_SUBABORT_END:
			return "SUB ABORT END";
		case TBLOCK_SUBABORT_PENDING:
			return "SUB ABRT PEND";
		case TBLOCK_SUBRESTART:
			return "SUB RESTART";
		case TBLOCK_SUBABORT_RESTART:
			return "SUB AB RESTRT";
	}
	return "UNRECOGNIZED";
}

/*
 * TransStateAsString
 *		Debug support
 */
static const char *
TransStateAsString(TransState state)
{
	switch (state)
	{
		case TRANS_DEFAULT:
			return "DEFAULT";
		case TRANS_START:
			return "START";
		case TRANS_INPROGRESS:
			return "INPROGR";
		case TRANS_COMMIT:
			return "COMMIT";
		case TRANS_ABORT:
			return "ABORT";
		case TRANS_PREPARE:
			return "PREPARE";
	}
	return "UNRECOGNIZED";
}

/*
 * EndLocalDistribXact
 *		Debug support
 */
static void
EndLocalDistribXact(bool isCommit)
{
	if (MyProc->localDistribXactData.state == LOCALDISTRIBXACT_STATE_NONE)
		return;

	/*
	 * MyProc->localDistribXactData is only used for debugging purpose by
	 * backend itself on segments only hence okay to modify without holding
	 * the lock.
	 */
	switch (DistributedTransactionContext)
	{
		case DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER:
		case DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER:
		case DTX_CONTEXT_QE_AUTO_COMMIT_IMPLICIT:
			LocalDistribXact_ChangeState(MyProc->pgprocno,
										 isCommit ?
										 LOCALDISTRIBXACT_STATE_COMMITTED :
										 LOCALDISTRIBXACT_STATE_ABORTED);
			break;

		case DTX_CONTEXT_QE_READER:
		case DTX_CONTEXT_QE_ENTRY_DB_SINGLETON:
			// QD or QE Writer will handle it.
			break;

		case DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE:
		case DTX_CONTEXT_QD_RETRY_PHASE_2:
		case DTX_CONTEXT_QE_PREPARED:
		case DTX_CONTEXT_QE_FINISH_PREPARED:
			elog(PANIC, "Unexpected distribute transaction context: '%s'",
				 DtxContextToString(DistributedTransactionContext));
			break;

		default:
			elog(PANIC, "Unrecognized DTX transaction context: %d",
				 (int) DistributedTransactionContext);
			break;
	}
}

/*
 * IsoLevelAsUpperString
 *		Formatting helper.
 */
const char *
IsoLevelAsUpperString(int IsoLevel)
{
	switch (IsoLevel)
	{
		case XACT_READ_UNCOMMITTED:
			return "READ UNCOMMITTED";
		case XACT_READ_COMMITTED:
			return "READ COMMITTED";
		case XACT_REPEATABLE_READ:
			return "REPEATABLE READ";
		case XACT_SERIALIZABLE:
			return "SERIALIZABLE";
		default:
			return "UNKNOWN";
	}
}


/*
 * xactGetCommittedChildren
 *
 * Gets the list of committed children of the current transaction.  The return
 * value is the number of child transactions.  *ptr is set to point to an
 * array of TransactionIds.  The array is allocated in TopTransactionContext;
 * the caller should *not* pfree() it (this is a change from pre-8.4 code!).
 * If there are no subxacts, *ptr is set to NULL.
 */
int
xactGetCommittedChildren(TransactionId **ptr)
{
	TransactionState s = CurrentTransactionState;

	if (s->nChildXids == 0)
		*ptr = NULL;
	else
		*ptr = s->childXids;

	return s->nChildXids;
}

/*
 *	XLOG support routines
 */

/*
 * Before 9.0 this was a fairly short function, but now it performs many
 * actions for which the order of execution is critical.
 */
static void
xact_redo_commit_internal(TransactionId xid, XLogRecPtr lsn,
						  TransactionId *sub_xids, int nsubxacts,
						  Oid tablespace_oid_to_delete,
						  SharedInvalidationMessage *inval_msgs, int nmsgs,
						  RelFileNodePendingDelete *xnodes, int nrels,
						  DbDirNode *deldbs, int ndeldbs,
						  Oid dbId, Oid tsId,
						  uint32 xinfo,
						  DistributedTransactionId distribXid,
						  DistributedTransactionTimeStamp distribTimeStamp)
{
	TransactionId max_xid;

	max_xid = TransactionIdLatest(xid, nsubxacts, sub_xids);

	ereportif(OidIsValid(tablespace_oid_to_delete), DEBUG5,
		(errmsg("in xact_redo_commit_internal with tablespace oid to delete: %u",
			tablespace_oid_to_delete)));

	/*
	 * Make sure nextXid is beyond any XID mentioned in the record.
	 *
	 * We don't expect anyone else to modify nextXid, hence we don't need to
	 * hold a lock while checking this. We still acquire the lock to modify
	 * it, though.
	 */
	if (TransactionIdFollowsOrEquals(max_xid,
									 ShmemVariableCache->nextXid))
	{
		LWLockAcquire(XidGenLock, LW_EXCLUSIVE);
		ShmemVariableCache->nextXid = max_xid;
		TransactionIdAdvance(ShmemVariableCache->nextXid);
		LWLockRelease(XidGenLock);
	}

	/* also update distributed commit log */
	if (distribXid != 0 && distribTimeStamp != 0)
	{
		DistributedLog_SetCommittedTree(xid, nsubxacts, sub_xids,
										distribTimeStamp, distribXid,
										/* isRedo */ true);
	}

	if (standbyState == STANDBY_DISABLED)
	{
		/*
		 * Mark the transaction committed in pg_clog.
		 */
		TransactionIdCommitTree(xid, nsubxacts, sub_xids);
	}
	else
	{
		/*
		 * If a transaction completion record arrives that has as-yet
		 * unobserved subtransactions then this will not have been fully
		 * handled by the call to RecordKnownAssignedTransactionIds() in the
		 * main recovery loop in xlog.c. So we need to do bookkeeping again to
		 * cover that case. This is confusing and it is easy to think this
		 * call is irrelevant, which has happened three times in development
		 * already. Leave it in.
		 */
		RecordKnownAssignedTransactionIds(max_xid);

		/*
		 * Mark the transaction committed in pg_clog. We use async commit
		 * protocol during recovery to provide information on database
		 * consistency for when users try to set hint bits. It is important
		 * that we do not set hint bits until the minRecoveryPoint is past
		 * this commit record. This ensures that if we crash we don't see hint
		 * bits set on changes made by transactions that haven't yet
		 * recovered. It's unlikely but it's good to be safe.
		 */
		TransactionIdAsyncCommitTree(xid, nsubxacts, sub_xids, lsn);

		/*
		 * We must mark clog before we update the ProcArray.
		 */
		ExpireTreeKnownAssignedTransactionIds(xid, nsubxacts, sub_xids, max_xid);

		/*
		 * Send any cache invalidations attached to the commit. We must
		 * maintain the same order of invalidation then release locks as
		 * occurs in CommitTransaction().
		 */
		ProcessCommittedInvalidationMessages(inval_msgs, nmsgs,
								  XactCompletionRelcacheInitFileInval(xinfo),
											 dbId, tsId);

		/*
		 * Release locks, if any. We do this for both two phase and normal one
		 * phase transactions. In effect we are ignoring the prepare phase and
		 * just going straight to lock release. At commit we release all locks
		 * via their top-level xid only, so no need to provide subxact list,
		 * which will save time when replaying commits.
		 */
		StandbyReleaseLockTree(xid, 0, NULL);
	}

	/* Make sure files supposed to be dropped are dropped */
	if (nrels > 0)
	{
		/*
		 * First update minimum recovery point to cover this WAL record. Once
		 * a relation is deleted, there's no going back. The buffer manager
		 * enforces the WAL-first rule for normal updates to relation files,
		 * so that the minimum recovery point is always updated before the
		 * corresponding change in the data file is flushed to disk, but we
		 * have to do the same here since we're bypassing the buffer manager.
		 *
		 * Doing this before deleting the files means that if a deletion fails
		 * for some reason, you cannot start up the system even after restart,
		 * until you fix the underlying situation so that the deletion will
		 * succeed. Alternatively, we could update the minimum recovery point
		 * after deletion, but that would leave a small window where the
		 * WAL-first rule would be violated.
		 */
		XLogFlush(lsn);

		/* Make sure files supposed to be dropped are dropped */
		DropRelationFiles(xnodes, nrels, true);
	}

	if (ndeldbs > 0)
	{
		XLogFlush(lsn);
		DropDatabaseDirectories(deldbs, ndeldbs, true);
	}

	DoTablespaceDeletionForRedoXlog(tablespace_oid_to_delete);

	/*
	 * We issue an XLogFlush() for the same reason we emit ForceSyncCommit()
	 * in normal operation. For example, in CREATE DATABASE, we copy all files
	 * from the template database, and then commit the transaction. If we
	 * crash after all the files have been copied but before the commit, you
	 * have files in the data directory without an entry in pg_database. To
	 * minimize the window for that, we use ForceSyncCommit() to rush the
	 * commit record to disk as quick as possible. We have the same window
	 * during recovery, and forcing an XLogFlush() (which updates
	 * minRecoveryPoint during recovery) helps to reduce that problem window,
	 * for any user that requested ForceSyncCommit().
	 */
	if (XactCompletionForceSyncCommit(xinfo))
		XLogFlush(lsn);

}

/*
 * Utility function to call xact_redo_commit_internal after breaking down xlrec
 */
static void
xact_redo_commit(xl_xact_commit *xlrec,
				 TransactionId xid, XLogRecPtr lsn,
				 DistributedTransactionId distribXid,
				 DistributedTransactionTimeStamp distribTimeStamp)
{
	TransactionId *subxacts;
	SharedInvalidationMessage *inval_msgs;
	DbDirNode *deldbs;
	/* subxid array follows relfilenodes */
	subxacts = (TransactionId *) &(xlrec->xnodes[xlrec->nrels]);
	/* invalidation messages array follows subxids */
	inval_msgs = (SharedInvalidationMessage *) &(subxacts[xlrec->nsubxacts]);
	/* database dir nodes to be deleted follow the invalidation messages */
	deldbs = (DbDirNode *) &(inval_msgs[xlrec->nmsgs]);

	xact_redo_commit_internal(xid, lsn, subxacts, xlrec->nsubxacts,
	  						  xlrec->tablespace_oid_to_delete_on_commit,
							  inval_msgs, xlrec->nmsgs,
							  xlrec->xnodes, xlrec->nrels,
							  deldbs, xlrec->ndeldbs,
							  xlrec->dbId,
							  xlrec->tsId,
							  xlrec->xinfo,
							  distribXid,
							  distribTimeStamp);
}

/*
 * Utility function to call xact_redo_commit_internal for compact form of message.
 */
static void
xact_redo_commit_compact(xl_xact_commit_compact *xlrec,
						 TransactionId xid, XLogRecPtr lsn)
{
	xact_redo_commit_internal(xid, lsn, xlrec->subxacts, xlrec->nsubxacts,
							  InvalidOid, /* no tablespace to delete */
							  NULL, 0,	/* inval msgs */
							  NULL, 0,	/* relfilenodes */
							  NULL, 0,	/* deldbs */
							  InvalidOid,		/* dbId */
							  InvalidOid,		/* tsId */
							  0,		/* xinfo */
							  0,		/* distribXid */
							  0);		/* distribTimeStamp */
}

/*
 * Be careful with the order of execution, as with xact_redo_commit().
 * The two functions are similar but differ in key places.
 *
 * Note also that an abort can be for a subtransaction and its children,
 * not just for a top level abort. That means we have to consider
 * topxid != xid, whereas in commit we would find topxid == xid always
 * because subtransaction commit is never WAL logged.
 */
static void
xact_redo_distributed_commit(xl_xact_commit *xlrec, TransactionId xid)
{
	TransactionId *sub_xids;
	SharedInvalidationMessage *msgs;
	DbDirNode *deldbs;
	TMGXACT_LOG *gxact_log;

	TransactionId max_xid;
	DistributedTransactionTimeStamp	distribTimeStamp;
	DistributedTransactionId 		distribXid;
	int			i;

	sub_xids = (TransactionId *) &xlrec->xnodes[xlrec->nrels];
	msgs = (SharedInvalidationMessage *) &sub_xids[xlrec->nsubxacts];
	deldbs = (DbDirNode *) &msgs[xlrec->nmsgs];
	gxact_log = (TMGXACT_LOG *) &deldbs[xlrec->ndeldbs];

	/*
	 * Crack open the gid to get the timestamp.
	 */
	dtxCrackOpenGid(
			gxact_log->gid,
			&distribTimeStamp,
			&distribXid);

	if (gxact_log->gxid != distribXid)
		elog(ERROR, 
		     "Distributed xid %u does not match %u in distributed commit redo record",
		     gxact_log->gxid, distribXid);

	if (TransactionIdIsValid(xid))
	{
		/*
		 * Mark the distributed transaction committed before we
		 * update the CLOG in xact_redo_commit.
		 */
		/*
		 * Make room in the DistributedLog, if necessary.
		 */
		if (TransactionIdFollowsOrEquals(xid,
										 ShmemVariableCache->nextXid))
		{
			ShmemVariableCache->nextXid = xid;
			TransactionIdAdvance(ShmemVariableCache->nextXid);
		}

		/*
		 * Now update the CLOG and do local commit actions.
		 *
		 * NOTE: The following logic is a copy of xact_redo_commit, with the
		 * only addition being redo of DistributeLog updates to subtransaction
		 * log.
		 */

		/* Mark the transaction committed in pg_clog */
		/* Add the committed subtransactions to the DistributedLog, too. */
		DistributedLog_SetCommittedTree(xid, xlrec->nsubxacts, sub_xids,
										distribTimeStamp,
										gxact_log->gxid,
										/* isRedo */ true);

		TransactionIdCommitTree(xid, xlrec->nsubxacts, sub_xids);

		/* Make sure nextXid is beyond any XID mentioned in the record */
		max_xid = xid;
		for (i = 0; i < xlrec->nsubxacts; i++)
		{
			if (TransactionIdPrecedes(max_xid, sub_xids[i]))
				max_xid = sub_xids[i];
		}
		if (TransactionIdFollowsOrEquals(max_xid,
										 ShmemVariableCache->nextXid))
		{
			ShmemVariableCache->nextXid = max_xid;
			TransactionIdAdvance(ShmemVariableCache->nextXid);
		}

		DropRelationFiles(xlrec->xnodes, xlrec->nrels, true);
		DropDatabaseDirectories(deldbs, xlrec->ndeldbs, true);
		DoTablespaceDeletionForRedoXlog(xlrec->tablespace_oid_to_delete_on_commit);
	}

	/*
	 * End copy of xact_redo_commit logic.
	 */
	redoDistributedCommitRecord(gxact_log);
}

static void
xact_redo_abort(xl_xact_abort *xlrec, TransactionId xid)
{
	TransactionId *sub_xids;
	TransactionId max_xid;
	DbDirNode *deldbs;

	sub_xids = (TransactionId *) &(xlrec->xnodes[xlrec->nrels]);
	max_xid = TransactionIdLatest(xid, xlrec->nsubxacts, sub_xids);
	deldbs = (DbDirNode *) &(sub_xids[xlrec->nsubxacts]);

	/*
	 * Make sure nextXid is beyond any XID mentioned in the record.
	 *
	 * We don't expect anyone else to modify nextXid, hence we don't need to
	 * hold a lock while checking this. We still acquire the lock to modify
	 * it, though.
	 */
	if (TransactionIdFollowsOrEquals(max_xid,
									 ShmemVariableCache->nextXid))
	{
		LWLockAcquire(XidGenLock, LW_EXCLUSIVE);
		ShmemVariableCache->nextXid = max_xid;
		TransactionIdAdvance(ShmemVariableCache->nextXid);
		LWLockRelease(XidGenLock);
	}

	if (standbyState == STANDBY_DISABLED)
	{
		/* Mark the transaction aborted in pg_clog, no need for async stuff */
		TransactionIdAbortTree(xid, xlrec->nsubxacts, sub_xids);
	}
	else
	{
		/*
		 * If a transaction completion record arrives that has as-yet
		 * unobserved subtransactions then this will not have been fully
		 * handled by the call to RecordKnownAssignedTransactionIds() in the
		 * main recovery loop in xlog.c. So we need to do bookkeeping again to
		 * cover that case. This is confusing and it is easy to think this
		 * call is irrelevant, which has happened three times in development
		 * already. Leave it in.
		 */
		RecordKnownAssignedTransactionIds(max_xid);

		/* Mark the transaction aborted in pg_clog, no need for async stuff */
		TransactionIdAbortTree(xid, xlrec->nsubxacts, sub_xids);

		/*
		 * We must update the ProcArray after we have marked clog.
		 */
		ExpireTreeKnownAssignedTransactionIds(xid, xlrec->nsubxacts, sub_xids, max_xid);

		/*
		 * There are no invalidation messages to send or undo.
		 */

		/*
		 * Release locks, if any. There are no invalidations to send.
		 */
		StandbyReleaseLockTree(xid, xlrec->nsubxacts, sub_xids);
	}

	/* Make sure files supposed to be dropped are dropped */
	DropRelationFiles(xlrec->xnodes, xlrec->nrels, true);
	DropDatabaseDirectories(deldbs, xlrec->ndeldbs, true);
	DoTablespaceDeletionForRedoXlog(xlrec->tablespace_oid_to_delete_on_abort);
}

static void
xact_redo_distributed_forget(xl_xact_distributed_forget *xlrec, TransactionId xid __attribute__((unused)) )
{
	redoDistributedForgetCommitRecord(&xlrec->gxact_log);
}


void
xact_redo(XLogRecPtr beginLoc __attribute__((unused)), XLogRecPtr lsn __attribute__((unused)), XLogRecord *record)
{
	uint8		info = record->xl_info & ~XLR_INFO_MASK;

	/* Backup blocks are not used in xact records */
	Assert(!(record->xl_info & XLR_BKP_BLOCK_MASK));

	if (info == XLOG_XACT_COMMIT_COMPACT)
	{
		xl_xact_commit_compact *xlrec = (xl_xact_commit_compact *) XLogRecGetData(record);

		xact_redo_commit_compact(xlrec, record->xl_xid, lsn);
	}
	else if (info == XLOG_XACT_COMMIT)
	{
		xl_xact_commit *xlrec = (xl_xact_commit *) XLogRecGetData(record);

		xact_redo_commit(xlrec, record->xl_xid, lsn, xlrec->distribXid, xlrec->distribTimeStamp);
	}
	else if (info == XLOG_XACT_ABORT)
	{
		xl_xact_abort *xlrec = (xl_xact_abort *) XLogRecGetData(record);

		xact_redo_abort(xlrec, record->xl_xid);
	}
	else if (info == XLOG_XACT_PREPARE)
	{
		/* the record contents are exactly the 2PC file */
		RecreateTwoPhaseFile(record->xl_xid,
							 XLogRecGetData(record), record->xl_len, &beginLoc);
	}
	else if (info == XLOG_XACT_COMMIT_PREPARED)
	{
		xl_xact_commit_prepared *xlrec = (xl_xact_commit_prepared *) XLogRecGetData(record);

		xact_redo_commit(&xlrec->crec, xlrec->xid, lsn,  xlrec->distribXid, xlrec->distribTimeStamp);
		RemoveTwoPhaseFile(xlrec->xid, false);
	}
	else if (info == XLOG_XACT_ABORT_PREPARED)
	{
		xl_xact_abort_prepared *xlrec = (xl_xact_abort_prepared *) XLogRecGetData(record);

		xact_redo_abort(&xlrec->arec, xlrec->xid);
		RemoveTwoPhaseFile(xlrec->xid, false);
	}
	else if (info == XLOG_XACT_DISTRIBUTED_COMMIT)
	{
		xl_xact_commit *xlrec = (xl_xact_commit *) XLogRecGetData(record);

		xact_redo_distributed_commit(xlrec, record->xl_xid);
	}
	else if (info == XLOG_XACT_DISTRIBUTED_FORGET)
	{
		xl_xact_distributed_forget *xlrec = (xl_xact_distributed_forget *) XLogRecGetData(record);

		xact_redo_distributed_forget(xlrec, record->xl_xid);
	}
	else if (info == XLOG_XACT_ASSIGNMENT)
	{
		xl_xact_assignment *xlrec = (xl_xact_assignment *) XLogRecGetData(record);

		if (standbyState >= STANDBY_INITIALIZED)
			ProcArrayApplyXidAssignment(xlrec->xtop,
										xlrec->nsubxacts, xlrec->xsub);
	}
	else
		elog(PANIC, "xact_redo: unknown op code %u", info);
}
