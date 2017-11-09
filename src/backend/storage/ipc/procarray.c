/*-------------------------------------------------------------------------
 *
 * procarray.c
 *	  POSTGRES process array code.
 *
 *
 * This module maintains an unsorted array of the PGPROC structures for all
 * active backends.  Although there are several uses for this, the principal
 * one is as a means of determining the set of currently running transactions.
 *
 * Because of various subtle race conditions it is critical that a backend
 * hold the correct locks while setting or clearing its MyProc->xid field.
 * See notes in src/backend/access/transam/README.
 *
 * The process array now also includes PGPROC structures representing
 * prepared transactions.  The xid and subxids fields of these are valid,
 * as are the myProcLocks lists.  They can be distinguished from regular
 * backend PGPROCs at need by checking for pid == 0.
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/storage/ipc/procarray.c,v 1.46 2008/08/04 18:03:46 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <signal.h>

#include "access/distributedlog.h"
#include "access/subtrans.h"
#include "access/transam.h"
#include "access/xact.h"
#include "access/twophase.h"
#include "miscadmin.h"
#include "storage/procarray.h"
#include "utils/combocid.h"
#include "utils/snapmgr.h"
#include "utils/tqual.h"
#include "utils/guc.h"
#include "utils/memutils.h"

#include "access/xact.h"		/* setting the shared xid */

#include "cdb/cdbtm.h"
#include "cdb/cdbvars.h"
#include "utils/faultinjector.h"
#include "utils/sharedsnapshot.h"

/* Our shared memory area */
typedef struct ProcArrayStruct
{
	int			numProcs;		/* number of valid procs entries */
	int			maxProcs;		/* allocated size of procs array */

	/*
	 * We declare procs[] as 1 entry because C wants a fixed-size array, but
	 * actually it is maxProcs entries long.
	 */
	PGPROC	   *procs[1];		/* VARIABLE LENGTH ARRAY */
} ProcArrayStruct;

static ProcArrayStruct *procArray;


#ifdef XIDCACHE_DEBUG

/* counters for XidCache measurement */
static long xc_by_recent_xmin = 0;
static long xc_by_known_xact = 0;
static long xc_by_my_xact = 0;
static long xc_by_latest_xid = 0;
static long xc_by_main_xid = 0;
static long xc_by_child_xid = 0;
static long xc_no_overflow = 0;
static long xc_slow_answer = 0;

#define xc_by_recent_xmin_inc()		(xc_by_recent_xmin++)
#define xc_by_known_xact_inc()		(xc_by_known_xact++)
#define xc_by_my_xact_inc()			(xc_by_my_xact++)
#define xc_by_latest_xid_inc()		(xc_by_latest_xid++)
#define xc_by_main_xid_inc()		(xc_by_main_xid++)
#define xc_by_child_xid_inc()		(xc_by_child_xid++)
#define xc_no_overflow_inc()		(xc_no_overflow++)
#define xc_slow_answer_inc()		(xc_slow_answer++)

static void DisplayXidCache(void);
#else							/* !XIDCACHE_DEBUG */

#define xc_by_recent_xmin_inc()		((void) 0)
#define xc_by_known_xact_inc()		((void) 0)
#define xc_by_my_xact_inc()			((void) 0)
#define xc_by_latest_xid_inc()		((void) 0)
#define xc_by_main_xid_inc()		((void) 0)
#define xc_by_child_xid_inc()		((void) 0)
#define xc_no_overflow_inc()		((void) 0)
#define xc_slow_answer_inc()		((void) 0)
#endif   /* XIDCACHE_DEBUG */


/*
 * Report shared-memory space needed by CreateSharedProcArray.
 */
Size
ProcArrayShmemSize(void)
{
	Size		size;

	size = offsetof(ProcArrayStruct, procs);
	size = add_size(size, mul_size(sizeof(PGPROC *),
								 add_size(MaxBackends, max_prepared_xacts)));

	return size;
}

/*
 * Initialize the shared PGPROC array during postmaster startup.
 */
void
CreateSharedProcArray(void)
{
	bool		found;

	/* Create or attach to the ProcArray shared structure */
	procArray = (ProcArrayStruct *)
		ShmemInitStruct("Proc Array", ProcArrayShmemSize(), &found);

	if (!found)
	{
		/*
		 * We're the first - initialize.
		 */
		procArray->numProcs = 0;
		procArray->maxProcs = MaxBackends + max_prepared_xacts;
	}
}

/*
 * Add the specified PGPROC to the shared array.
 */
void
ProcArrayAdd(PGPROC *proc)
{
	ProcArrayStruct *arrayP = procArray;

	LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

	SIMPLE_FAULT_INJECTOR(ProcArray_Add);

	if (arrayP->numProcs >= arrayP->maxProcs)
	{
		/*
		 * Ooops, no room.	(This really shouldn't happen, since there is a
		 * fixed supply of PGPROC structs too, and so we should have failed
		 * earlier.)
		 */
		LWLockRelease(ProcArrayLock);
		ereport(FATAL,
				(errcode(ERRCODE_TOO_MANY_CONNECTIONS),
				 errmsg("sorry, too many clients already")));
	}

	arrayP->procs[arrayP->numProcs] = proc;
	arrayP->numProcs++;

	LWLockRelease(ProcArrayLock);
}

/*
 * Remove the specified PGPROC from the shared array.
 *
 * When latestXid is a valid XID, we are removing a live 2PC gxact from the
 * array, and thus causing it to appear as "not running" anymore.  In this
 * case we must advance latestCompletedXid.  (This is essentially the same
 * as ProcArrayEndTransaction followed by removal of the PGPROC, but we take
 * the ProcArrayLock only once, and don't damage the content of the PGPROC;
 * twophase.c depends on the latter.)
 */
void
ProcArrayRemove(PGPROC *proc, TransactionId latestXid)
{
	ProcArrayStruct *arrayP = procArray;
	int			index;

#ifdef XIDCACHE_DEBUG
	/* dump stats at backend shutdown, but not prepared-xact end */
	if (proc->pid != 0)
		DisplayXidCache();
#endif

	LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

	if (TransactionIdIsValid(latestXid))
	{
		Assert(TransactionIdIsValid(proc->xid));

		/* Advance global latestCompletedXid while holding the lock */
		if (TransactionIdPrecedes(ShmemVariableCache->latestCompletedXid,
								  latestXid))
			ShmemVariableCache->latestCompletedXid = latestXid;
	}
	else
	{
		/* Shouldn't be trying to remove a live transaction here */
		Assert(!TransactionIdIsValid(proc->xid));
	}

	for (index = 0; index < arrayP->numProcs; index++)
	{
		if (arrayP->procs[index] == proc)
		{
			arrayP->procs[index] = arrayP->procs[arrayP->numProcs - 1];
			arrayP->procs[arrayP->numProcs - 1] = NULL; /* for debugging */
			arrayP->numProcs--;
			LWLockRelease(ProcArrayLock);
			return;
		}
	}

	/* Ooops */
	LWLockRelease(ProcArrayLock);

	elog(LOG, "failed to find proc %p in ProcArray", proc);
}


/*
 * ProcArrayEndTransaction -- mark a transaction as no longer running
 *
 * This is used interchangeably for commit and abort cases.  The transaction
 * commit/abort must already be reported to WAL and pg_clog.
 *
 * proc is currently always MyProc, but we pass it explicitly for flexibility.
 * latestXid is the latest Xid among the transaction's main XID and
 * subtransactions, or InvalidTransactionId if it has no XID.  (We must ask
 * the caller to pass latestXid, instead of computing it from the PGPROC's
 * contents, because the subxid information in the PGPROC might be
 * incomplete.)
 *
 * GPDB: If this is a global transaction, we might need to do this action
 * later, rather than now. In that case, this function returns true for
 * needNotifyCommittedDtxTransaction, and does *not* change the state of the
 * PGPROC entry. This can only happen for commit; when !isCommit, this always
 * clears the PGPROC entry.
 */
bool
ProcArrayEndTransaction(PGPROC *proc, TransactionId latestXid, bool isCommit)
{
	bool needNotifyCommittedDtxTransaction;

	/*
	 * MyProc->localDistribXactData is only used for debugging purpose by
	 * backend itself on segments only hence okay to modify without holding
	 * the lock.
	 */
	if (MyProc->localDistribXactData.state != LOCALDISTRIBXACT_STATE_NONE)
	{
		switch (DistributedTransactionContext)
		{
			case DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER:
			case DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER:
			case DTX_CONTEXT_QE_AUTO_COMMIT_IMPLICIT:
				LocalDistribXact_ChangeState(MyProc,
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

			default:
				elog(PANIC, "Unrecognized DTX transaction context: %d",
					 (int) DistributedTransactionContext);
		}
	}

	if (isCommit && notifyCommittedDtxTransactionIsNeeded())
		needNotifyCommittedDtxTransaction = true;
	else
		needNotifyCommittedDtxTransaction = false;
	
	if (TransactionIdIsValid(latestXid))
	{
		LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);
		/*
		 * We must lock ProcArrayLock while clearing proc->xid, so that we do
		 * not exit the set of "running" transactions while someone else is
		 * taking a snapshot.  See discussion in
		 * src/backend/access/transam/README.
		 */
		Assert(TransactionIdIsValid(proc->xid) ||
			   (IsBootstrapProcessingMode() && latestXid == BootstrapTransactionId));

		if (! needNotifyCommittedDtxTransaction)
		{
			proc->xid = InvalidTransactionId;
			proc->lxid = InvalidLocalTransactionId;
			proc->xmin = InvalidTransactionId;
			/* must be cleared with xid/xmin: */
			proc->vacuumFlags &= ~PROC_VACUUM_STATE_MASK;
			proc->inCommit = false; /* be sure this is cleared in abort */
			proc->serializableIsoLevel = false;
			proc->inDropTransaction = false;

			/* Clear the subtransaction-XID cache too while holding the lock */
			proc->subxids.nxids = 0;
			proc->subxids.overflowed = false;
		}

		/* Also advance global latestCompletedXid while holding the lock */
		/*
		 * Note: we do this in GPDB even if we didn't clear our XID entry
		 * just yet. There is no harm in advancing latestCompletedXid a
		 * little bit earlier than strictly necessary, and this way we don't
		 * need to remember out latest XID when we later actually clear the
		 * entry.
		 */
		if (TransactionIdPrecedes(ShmemVariableCache->latestCompletedXid,
								  latestXid))
			ShmemVariableCache->latestCompletedXid = latestXid;

		LWLockRelease(ProcArrayLock);
	}
	else
	{
		/*
		 * If we have no XID, we don't need to lock, since we won't affect
		 * anyone else's calculation of a snapshot.  We might change their
		 * estimate of global xmin, but that's OK.
		 */
		Assert(!TransactionIdIsValid(proc->xid));

		proc->lxid = InvalidLocalTransactionId;
		proc->xmin = InvalidTransactionId;
		/* must be cleared with xid/xmin: */
		proc->vacuumFlags &= ~PROC_VACUUM_STATE_MASK;
		proc->inCommit = false; /* be sure this is cleared in abort */
		proc->serializableIsoLevel = false;
		proc->inDropTransaction = false;

		Assert(proc->subxids.nxids == 0);
		Assert(proc->subxids.overflowed == false);
	}

	return needNotifyCommittedDtxTransaction;
}


/*
 * ProcArrayClearTransaction -- clear the transaction fields
 *
 * This is used after successfully preparing a 2-phase transaction.  We are
 * not actually reporting the transaction's XID as no longer running --- it
 * will still appear as running because the 2PC's gxact is in the ProcArray
 * too.  We just have to clear out our own PGPROC.
 *
 */
void
ProcArrayClearTransaction(PGPROC *proc, bool commit)
{
	/*
	 * We can skip locking ProcArrayLock here, because this action does not
	 * actually change anyone's view of the set of running XIDs: our entry is
	 * duplicate with the gxact that has already been inserted into the
	 * ProcArray.
	 */
	proc->xid = InvalidTransactionId;
	proc->xmin = InvalidTransactionId;

	proc->localDistribXactData.state = LOCALDISTRIBXACT_STATE_NONE;

	/* redundant, but just in case */
	proc->vacuumFlags &= ~PROC_VACUUM_STATE_MASK;
	proc->serializableIsoLevel = false;
	proc->inDropTransaction = false;

	/* Clear the subtransaction-XID cache too */
	proc->subxids.nxids = 0;
	proc->subxids.overflowed = false;

	/* For commit, inCommit and lxid are cleared in CommitTransaction after
	 * performing PT operations. It's done this way to correctly block
	 * checkpoint till CommitTransaction completes the persistent table
	 * updates.
	 */
	if (! commit)
	{
		proc->lxid = InvalidLocalTransactionId;
		proc->inCommit = false;
	}
}

/*
 * Clears the current transaction from PGPROC.
 *
 * Must be called while holding the ProcArrayLock.
 */
void
ClearTransactionFromPgProc_UnderLock(PGPROC *proc, bool commit)
{
	/*
	 * ProcArrayClearTransaction() doesn't take the lock, so we can just call it
	 * directly.
	 */
	ProcArrayClearTransaction(proc, commit);
}

/*
 * TransactionIdIsInProgress -- is given transaction running in some backend
 *
 * Aside from some shortcuts such as checking RecentXmin and our own Xid,
 * there are three possibilities for finding a running transaction:
 *
 * 1. the given Xid is a main transaction Id.  We will find this out cheaply
 * by looking at the PGPROC struct for each backend.
 *
 * 2. the given Xid is one of the cached subxact Xids in the PGPROC array.
 * We can find this out cheaply too.
 *
 * 3. Search the SubTrans tree to find the Xid's topmost parent, and then
 * see if that is running according to PGPROC.	This is the slowest, but
 * sadly it has to be done always if the other two failed, unless we see
 * that the cached subxact sets are complete (none have overflowed).
 *
 * ProcArrayLock has to be held while we do 1 and 2.  If we save the top Xids
 * while doing 1, we can release the ProcArrayLock while we do 3.  This buys
 * back some concurrency (we can't retrieve the main Xids from PGPROC again
 * anyway; see GetNewTransactionId).
 */
bool
TransactionIdIsInProgress(TransactionId xid)
{
	static TransactionId *xids = NULL;
	int			nxids = 0;
	ProcArrayStruct *arrayP = procArray;
	TransactionId topxid;
	int			i,
				j;

	/*
	 * Don't bother checking a transaction older than RecentXmin; it could not
	 * possibly still be running.  (Note: in particular, this guarantees that
	 * we reject InvalidTransactionId, FrozenTransactionId, etc as not
	 * running.)
	 */
	if (TransactionIdPrecedes(xid, RecentXmin))
	{
		xc_by_recent_xmin_inc();
		return false;
	}

	/*
	 * We may have just checked the status of this transaction, so if it is
	 * already known to be completed, we can fall out without any access to
	 * shared memory.
	 */
	if (TransactionIdIsKnownCompleted(xid))
	{
		xc_by_known_xact_inc();
		return false;
	}

	/*
	 * Also, we can handle our own transaction (and subtransactions) without
	 * any access to shared memory.
	 */
	if (TransactionIdIsCurrentTransactionId(xid))
	{
		xc_by_my_xact_inc();
		return true;
	}

	/*
	 * If not first time through, get workspace to remember main XIDs in. We
	 * malloc it permanently to avoid repeated palloc/pfree overhead.
	 */
	if (xids == NULL)
	{
		xids = (TransactionId *)
			malloc(arrayP->maxProcs * sizeof(TransactionId));
		if (xids == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 errmsg("out of memory")));
	}

	LWLockAcquire(ProcArrayLock, LW_SHARED);

	/*
	 * Now that we have the lock, we can check latestCompletedXid; if the
	 * target Xid is after that, it's surely still running.
	 */
	if (TransactionIdPrecedes(ShmemVariableCache->latestCompletedXid, xid))
	{
		LWLockRelease(ProcArrayLock);
		xc_by_latest_xid_inc();
		return true;
	}

	/* No shortcuts, gotta grovel through the array */
	for (i = 0; i < arrayP->numProcs; i++)
	{
		volatile PGPROC *proc = arrayP->procs[i];
		TransactionId pxid;

		/* Ignore my own proc --- dealt with it above */
		if (proc == MyProc)
			continue;

		/* Fetch xid just once - see GetNewTransactionId */
		pxid = proc->xid;

		if (!TransactionIdIsValid(pxid))
			continue;

		/*
		 * Step 1: check the main Xid
		 */
		if (TransactionIdEquals(pxid, xid))
		{
			LWLockRelease(ProcArrayLock);
			xc_by_main_xid_inc();
			return true;
		}

		/*
		 * We can ignore main Xids that are younger than the target Xid, since
		 * the target could not possibly be their child.
		 */
		if (TransactionIdPrecedes(xid, pxid))
			continue;

		/*
		 * Step 2: check the cached child-Xids arrays
		 */
		for (j = proc->subxids.nxids - 1; j >= 0; j--)
		{
			/* Fetch xid just once - see GetNewTransactionId */
			TransactionId cxid = proc->subxids.xids[j];

			if (TransactionIdEquals(cxid, xid))
			{
				LWLockRelease(ProcArrayLock);
				xc_by_child_xid_inc();
				return true;
			}
		}

		/*
		 * Save the main Xid for step 3.  We only need to remember main Xids
		 * that have uncached children.  (Note: there is no race condition
		 * here because the overflowed flag cannot be cleared, only set, while
		 * we hold ProcArrayLock.  So we can't miss an Xid that we need to
		 * worry about.)
		 */
		if (proc->subxids.overflowed)
			xids[nxids++] = pxid;
	}

	LWLockRelease(ProcArrayLock);

	/*
	 * If none of the relevant caches overflowed, we know the Xid is not
	 * running without looking at pg_subtrans.
	 */
	if (nxids == 0)
	{
		xc_no_overflow_inc();
		return false;
	}

	/*
	 * Step 3: have to check pg_subtrans.
	 *
	 * At this point, we know it's either a subtransaction of one of the Xids
	 * in xids[], or it's not running.  If it's an already-failed
	 * subtransaction, we want to say "not running" even though its parent may
	 * still be running.  So first, check pg_clog to see if it's been aborted.
	 */
	xc_slow_answer_inc();

	if (TransactionIdDidAbort(xid))
		return false;

	/*
	 * It isn't aborted, so check whether the transaction tree it belongs to
	 * is still running (or, more precisely, whether it was running when we
	 * held ProcArrayLock).
	 */
	topxid = SubTransGetTopmostTransaction(xid);
	Assert(TransactionIdIsValid(topxid));
	if (!TransactionIdEquals(topxid, xid))
	{
		for (i = 0; i < nxids; i++)
		{
			if (TransactionIdEquals(xids[i], topxid))
				return true;
		}
	}

	return false;
}

/*
 * TransactionIdIsActive -- is xid the top-level XID of an active backend?
 *
 * This differs from TransactionIdIsInProgress in that it ignores prepared
 * transactions.  Also, we ignore subtransactions since that's not needed
 * for current uses.
 */
bool
TransactionIdIsActive(TransactionId xid)
{
	bool		result = false;
	ProcArrayStruct *arrayP = procArray;
	int			i;

	/*
	 * Don't bother checking a transaction older than RecentXmin; it could not
	 * possibly still be running.
	 */
	if (TransactionIdPrecedes(xid, RecentXmin))
		return false;

	LWLockAcquire(ProcArrayLock, LW_SHARED);

	for (i = 0; i < arrayP->numProcs; i++)
	{
		volatile PGPROC *proc = arrayP->procs[i];

		/* Fetch xid just once - see GetNewTransactionId */
		TransactionId pxid = proc->xid;

		if (!TransactionIdIsValid(pxid))
			continue;

		if (proc->pid == 0)
			continue;			/* ignore prepared transactions */

		if (TransactionIdEquals(pxid, xid))
		{
			result = true;
			break;
		}
	}

	LWLockRelease(ProcArrayLock);

	return result;
}

/*
 * Returns true if there are any UAO drop transaction active (except the current
 * one).
 *
 * If allDbs is TRUE then all backends are considered; if allDbs is FALSE
 * then only backends running in my own database are considered.
 */
bool
HasDropTransaction(bool allDbs)
{
	ProcArrayStruct *arrayP = procArray;
	bool result = false; /* Assumes */
	int			index;

	LWLockAcquire(ProcArrayLock, LW_SHARED);

	for (index = 0; index < arrayP->numProcs; index++)
	{
		volatile PGPROC *proc = arrayP->procs[index];
		if (proc->pid == 0)
			continue;			/* do not count prepared xacts */

		if (allDbs || proc->databaseId == MyDatabaseId)
		{
			if (proc->inDropTransaction && proc != MyProc)
			{
				ereport((Debug_print_snapshot_dtm ? LOG : DEBUG3),
						(errmsg("Found drop transaction: database %d, pid %d, xid %d, xmin %d",
								proc->databaseId, proc->pid, proc->xid, proc->xmin)));
				result = true;
			}
		}
	}

	LWLockRelease(ProcArrayLock);

	return result;
}

/*
 * Returns true if there are of serializable backends (except the current
 * one).
 *
 * If allDbs is TRUE then all backends are considered; if allDbs is FALSE
 * then only backends running in my own database are considered.
 */
bool
HasSerializableBackends(bool allDbs)
{
	ProcArrayStruct *arrayP = procArray;
	bool result = false; /* Assumes */
	int			index;

	LWLockAcquire(ProcArrayLock, LW_SHARED);

	for (index = 0; index < arrayP->numProcs; index++)
	{
		volatile PGPROC *proc = arrayP->procs[index];
		if (proc->pid == 0)
			continue;			/* do not count prepared xacts */

		if (allDbs || proc->databaseId == MyDatabaseId)
		{
			if (proc->serializableIsoLevel && proc != MyProc)
			{
				ereport((Debug_print_snapshot_dtm ? LOG : DEBUG3),
						(errmsg("Found serializable transaction: database %d, pid %d, xid %d, xmin %d",
								proc->databaseId, proc->pid, proc->xid, proc->xmin)));
				result = true;
			}
		}
	}

	LWLockRelease(ProcArrayLock);

	return result;
}

/*
 * GetOldestXmin -- returns oldest transaction that was running
 *					when any current transaction was started.
 *
 * If allDbs is TRUE then all backends are considered; if allDbs is FALSE
 * then only backends running in my own database are considered.
 *
 * This is used by VACUUM to decide which deleted tuples must be preserved
 * in a table.	allDbs = TRUE is needed for shared relations, but allDbs =
 * FALSE is sufficient for non-shared relations, since only backends in my
 * own database could ever see the tuples in them.	Also, we can ignore
 * concurrently running lazy VACUUMs because (a) they must be working on other
 * tables, and (b) they don't need to do snapshot-based lookups.
 *
 * This is also used to determine where to truncate pg_subtrans.  allDbs
 * must be TRUE for that case, and ignoreVacuum FALSE.
 *
 * GPDB: ignoreVacuum is ignored.
 *
 * Note: we include all currently running xids in the set of considered xids.
 * This ensures that if a just-started xact has not yet set its snapshot,
 * when it does set the snapshot it cannot set xmin less than what we compute.
 * See notes in src/backend/access/transam/README.
 */
TransactionId
GetOldestXmin(bool allDbs, bool ignoreVacuum)
{
	ProcArrayStruct *arrayP = procArray;
	TransactionId result;
	int			index;

	LWLockAcquire(ProcArrayLock, LW_SHARED);

	/*
	 * We initialize the MIN() calculation with latestCompletedXid + 1. This
	 * is a lower bound for the XIDs that might appear in the ProcArray later,
	 * and so protects us against overestimating the result due to future
	 * additions.
	 */
	result = ShmemVariableCache->latestCompletedXid;
	Assert(TransactionIdIsNormal(result));
	TransactionIdAdvance(result);

	for (index = 0; index < arrayP->numProcs; index++)
	{
		volatile PGPROC *proc = arrayP->procs[index];

		if (allDbs || proc->databaseId == MyDatabaseId)
		{
			/* Fetch xid just once - see GetNewTransactionId */
			TransactionId xid = proc->xid;

			/* First consider the transaction's own Xid, if any */
			if (TransactionIdIsNormal(xid) &&
				TransactionIdPrecedes(xid, result))
				result = xid;

			/*
			 * Also consider the transaction's Xmin, if set.
			 *
			 * We must check both Xid and Xmin because a transaction might
			 * have an Xmin but not (yet) an Xid; conversely, if it has an
			 * Xid, that could determine some not-yet-set Xmin.
			 */
			xid = proc->xmin;	/* Fetch just once */
			if (TransactionIdIsNormal(xid) &&
				TransactionIdPrecedes(xid, result))
				result = xid;
		}
	}

	LWLockRelease(ProcArrayLock);

	return result;
}

void
updateSharedLocalSnapshot(DtxContextInfo *dtxContextInfo, Snapshot snapshot, char *debugCaller)
{
	int combocidSize;

	Assert(SharedLocalSnapshotSlot != NULL);

	Assert(snapshot != NULL);

	ereport((Debug_print_full_dtm ? LOG : DEBUG5),
			(errmsg("updateSharedLocalSnapshot for DistributedTransactionContext = '%s' passed local snapshot (xmin: %u xmax: %u xcnt: %u) curcid: %d",
					DtxContextToString(DistributedTransactionContext),
					snapshot->xmin,
					snapshot->xmax,
					snapshot->xcnt,
					snapshot->curcid)));

	LWLockAcquire(SharedLocalSnapshotSlot->slotLock, LW_EXCLUSIVE);

	SharedLocalSnapshotSlot->snapshot.xmin = snapshot->xmin;
	SharedLocalSnapshotSlot->snapshot.xmax = snapshot->xmax;
	SharedLocalSnapshotSlot->snapshot.xcnt = snapshot->xcnt;

	if (snapshot->xcnt > 0)
	{
		Assert(snapshot->xip != NULL);

		ereport((Debug_print_full_dtm ? LOG : DEBUG5),
				(errmsg("updateSharedLocalSnapshot count of in-doubt ids %u",
						SharedLocalSnapshotSlot->snapshot.xcnt)));

		memcpy(SharedLocalSnapshotSlot->snapshot.xip, snapshot->xip, snapshot->xcnt * sizeof(TransactionId));
	}
	
	/* combocid stuff */
	combocidSize = ((usedComboCids < MaxComboCids) ? usedComboCids : MaxComboCids );

	SharedLocalSnapshotSlot->combocidcnt = combocidSize;	
	memcpy((void *)SharedLocalSnapshotSlot->combocids, comboCids,
		   combocidSize * sizeof(ComboCidKeyData));

	SharedLocalSnapshotSlot->snapshot.curcid = snapshot->curcid;

	ereport((Debug_print_full_dtm ? LOG : DEBUG5),
			(errmsg("updateSharedLocalSnapshot: combocidsize is now %d max %d segmateSync %d->%d",
					combocidSize, MaxComboCids, SharedLocalSnapshotSlot->segmateSync, dtxContextInfo->segmateSync)));

	SetSharedTransactionId_writer();
	
	SharedLocalSnapshotSlot->QDcid = dtxContextInfo->curcid;
	SharedLocalSnapshotSlot->QDxid = dtxContextInfo->distributedXid;
		
	SharedLocalSnapshotSlot->ready = true;

	SharedLocalSnapshotSlot->segmateSync = dtxContextInfo->segmateSync;

	ereport((Debug_print_full_dtm ? LOG : DEBUG5),
			(errmsg("updateSharedLocalSnapshot for DistributedTransactionContext = '%s' setting shared local snapshot xid = %u (xmin: %u xmax: %u xcnt: %u) curcid: %d, QDxid = %u, QDcid = %u",
					DtxContextToString(DistributedTransactionContext),
					SharedLocalSnapshotSlot->xid,
					SharedLocalSnapshotSlot->snapshot.xmin,
					SharedLocalSnapshotSlot->snapshot.xmax,
					SharedLocalSnapshotSlot->snapshot.xcnt,
					SharedLocalSnapshotSlot->snapshot.curcid,
					SharedLocalSnapshotSlot->QDxid,
					SharedLocalSnapshotSlot->QDcid)));

	ereport((Debug_print_snapshot_dtm ? LOG : DEBUG5),
			(errmsg("[Distributed Snapshot #%u] *Writer Set Shared* gxid %u, currcid %d (gxid = %u, slot #%d, '%s', '%s')",
					QEDtxContextInfo.distributedSnapshot.distribSnapshotId,
					SharedLocalSnapshotSlot->QDxid,
					SharedLocalSnapshotSlot->QDcid,
					getDistributedTransactionId(),
					SharedLocalSnapshotSlot->slotid,
					debugCaller,
					DtxContextToString(DistributedTransactionContext))));
	LWLockRelease(SharedLocalSnapshotSlot->slotLock);
}

static int
GetDistributedSnapshotMaxCount(void)
{
	switch (DistributedTransactionContext)
	{
	case DTX_CONTEXT_LOCAL_ONLY:
	case DTX_CONTEXT_QD_RETRY_PHASE_2:
	case DTX_CONTEXT_QE_FINISH_PREPARED:
		return 0;

	case DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE:
		return max_prepared_xacts;

	case DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER:
	case DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER:
	case DTX_CONTEXT_QE_AUTO_COMMIT_IMPLICIT:
	case DTX_CONTEXT_QE_ENTRY_DB_SINGLETON:
	case DTX_CONTEXT_QE_READER:
		if (QEDtxContextInfo.distributedSnapshot.distribSnapshotId != 0)
			return QEDtxContextInfo.distributedSnapshot.maxCount;
		else
			return max_prepared_xacts;		/* UNDONE: For now? */
	
	case DTX_CONTEXT_QE_PREPARED:
		elog(FATAL, "Unexpected segment distribute transaction context: '%s'",
			 DtxContextToString(DistributedTransactionContext));
		break;
	
	default:
		elog(FATAL, "Unrecognized DTX transaction context: %d",
			(int) DistributedTransactionContext);
		break;
	}

	return 0;
}

/*
 * Fill in the array of in-progress distributed XIDS in 'snapshot' from the
 * information that the QE sent us (if any).
 */
static void
FillInDistributedSnapshot(Snapshot snapshot)
{
	ereport((Debug_print_full_dtm ? LOG : DEBUG5),
			(errmsg("FillInDistributedSnapshot DTX Context = '%s'",
					DtxContextToString(DistributedTransactionContext))));

	switch (DistributedTransactionContext)
	{
	case DTX_CONTEXT_LOCAL_ONLY:
	case DTX_CONTEXT_QD_RETRY_PHASE_2:
	case DTX_CONTEXT_QE_FINISH_PREPARED:
		/*
		 * No distributed snapshot.
		 */
		snapshot->haveDistribSnapshot = false;
		snapshot->distribSnapshotWithLocalMapping.ds.count = 0;
		break;

	case DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE:
		/*
		 * GetSnapshotData() should've acquired the distributed snapshot
		 * while holding ProcArrayLock, not here.
		 */
		elog(ERROR, "FillInDistributedSnapshot called in context '%s'",
			 DtxContextToString(DistributedTransactionContext));
		break;

	case DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER:
	case DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER:
	case DTX_CONTEXT_QE_AUTO_COMMIT_IMPLICIT:
	case DTX_CONTEXT_QE_ENTRY_DB_SINGLETON:
	case DTX_CONTEXT_QE_READER:
		/*
		 * Copy distributed snapshot from the one sent by the QD.
		 */
		{
			DistributedSnapshot *ds = &QEDtxContextInfo.distributedSnapshot;

			if (ds->distribSnapshotId != 0)
			{
				snapshot->haveDistribSnapshot = true;

				Assert(ds->xminAllDistributedSnapshots);
				Assert(ds->xminAllDistributedSnapshots <= ds->xmin);

				DistributedSnapshot_Copy(&snapshot->distribSnapshotWithLocalMapping.ds, ds);
			}
			else
			{
				snapshot->haveDistribSnapshot = false;
				snapshot->distribSnapshotWithLocalMapping.ds.count = 0;
			}
		}
		break;

	case DTX_CONTEXT_QE_PREPARED:
		elog(FATAL, "Unexpected segment distribute transaction context: '%s'",
			 DtxContextToString(DistributedTransactionContext));
		break;

	default:
		elog(FATAL, "Unrecognized DTX transaction context: %d",
			(int) DistributedTransactionContext);
		break;
	}

	/*
	 * Nice that we may have collected it, but turn it off...
	 */
	if (Debug_disable_distributed_snapshot)
		snapshot->haveDistribSnapshot = false;
}

/*
 * QEDtxContextInfo and SharedLocalSnapshotSlot are both global.
 */
static bool
QEwriterSnapshotUpToDate(void)
{
	Assert(!Gp_is_writer);

	if (SharedLocalSnapshotSlot == NULL)
		elog(ERROR, "SharedLocalSnapshotSlot is NULL");

	LWLockAcquire(SharedLocalSnapshotSlot->slotLock, LW_SHARED);
	bool result = QEDtxContextInfo.distributedXid == SharedLocalSnapshotSlot->QDxid &&
		QEDtxContextInfo.curcid == SharedLocalSnapshotSlot->QDcid &&
		QEDtxContextInfo.segmateSync == SharedLocalSnapshotSlot->segmateSync &&
		SharedLocalSnapshotSlot->ready;
	LWLockRelease(SharedLocalSnapshotSlot->slotLock);

	return result;
}

/*----------
 * GetSnapshotData -- returns information about running transactions.
 *
 * The returned snapshot includes xmin (lowest still-running xact ID),
 * xmax (highest completed xact ID + 1), and a list of running xact IDs
 * in the range xmin <= xid < xmax.  It is used as follows:
 *		All xact IDs < xmin are considered finished.
 *		All xact IDs >= xmax are considered still running.
 *		For an xact ID xmin <= xid < xmax, consult list to see whether
 *		it is considered running or not.
 * This ensures that the set of transactions seen as "running" by the
 * current xact will not change after it takes the snapshot.
 *
 * All running top-level XIDs are included in the snapshot, except for lazy
 * VACUUM processes.  We also try to include running subtransaction XIDs,
 * but since PGPROC has only a limited cache area for subxact XIDs, full
 * information may not be available.  If we find any overflowed subxid arrays,
 * we have to mark the snapshot's subxid data as overflowed, and extra work
 * will need to be done to determine what's running (see XidInMVCCSnapshot()
 * in tqual.c).
 *
 * We also update the following backend-global variables:
 *		TransactionXmin: the oldest xmin of any snapshot in use in the
 *			current transaction (this is the same as MyProc->xmin).
 *		RecentXmin: the xmin computed for the most recent snapshot.  XIDs
 *			older than this are known not running any more.
 *		RecentGlobalXmin: the global xmin (oldest TransactionXmin across all
 *			running transactions, except those running LAZY VACUUM).  This is
 *			the same computation done by GetOldestXmin(true, true).
 *
 * Note: this function should probably not be called with an argument that's
 * not statically allocated (see xip allocation below).
 */
Snapshot
GetSnapshotData(Snapshot snapshot)
{
	ProcArrayStruct *arrayP = procArray;
	TransactionId xmin;
	TransactionId xmax;
	TransactionId globalxmin;
	int			index;
	int			count = 0;
	int			subcount = 0;

	Assert(snapshot != NULL);

	/*
	 * Allocating space for maxProcs xids is usually overkill; numProcs would
	 * be sufficient.  But it seems better to do the malloc while not holding
	 * the lock, so we can't look at numProcs.  Likewise, we allocate much
	 * more subxip storage than is probably needed.
	 *
	 * This does open a possibility for avoiding repeated malloc/free: since
	 * maxProcs does not change at runtime, we can simply reuse the previous
	 * xip arrays if any.  (This relies on the fact that all callers pass
	 * static SnapshotData structs.)
	 */
	if (snapshot->xip == NULL)
	{
		/*
		 * First call for this snapshot
		 */
		snapshot->xip = (TransactionId *)
			malloc(arrayP->maxProcs * sizeof(TransactionId));
		if (snapshot->xip == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 errmsg("out of memory")));

		Assert(snapshot->subxip == NULL);
	}

	if (snapshot->subxip == NULL)
	{
		snapshot->subxip = (TransactionId *)
			malloc(arrayP->maxProcs * PGPROC_MAX_CACHED_SUBXIDS * sizeof(TransactionId));
		if (snapshot->subxip == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 errmsg("out of memory")));
	}

	/*
	 * GP: Distributed snapshot.
	 */
	ereport((Debug_print_full_dtm ? LOG : DEBUG5),
			(errmsg("GetSnapshotData maxCount %d, inProgressEntryArray %p",
					snapshot->distribSnapshotWithLocalMapping.ds.maxCount,
					snapshot->distribSnapshotWithLocalMapping.ds.inProgressXidArray)));

	if (snapshot->distribSnapshotWithLocalMapping.ds.inProgressXidArray == NULL)
	{
		int maxCount = GetDistributedSnapshotMaxCount();
		if (maxCount > 0)
		{
			snapshot->distribSnapshotWithLocalMapping.ds.inProgressXidArray =
				(DistributedTransactionId*)malloc(maxCount * sizeof(DistributedTransactionId));
			if (snapshot->distribSnapshotWithLocalMapping.ds.inProgressXidArray == NULL)
			{
				ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
			}
			snapshot->distribSnapshotWithLocalMapping.ds.maxCount = maxCount;

			/*
			 * Allocate memory for local xid cache, currently allocating it
			 * same size as distributed, not necessary.
			 */
			snapshot->distribSnapshotWithLocalMapping.inProgressMappedLocalXids =
				(TransactionId*)malloc(maxCount * sizeof(TransactionId));
			if (snapshot->distribSnapshotWithLocalMapping.inProgressMappedLocalXids == NULL)
			{
				ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
			}
			snapshot->distribSnapshotWithLocalMapping.maxLocalXidsCount = maxCount;
		}
	}

	/*
	 * MPP Addition. if we are in EXECUTE mode and not the writer... then we
	 * want to just get the shared snapshot and make it our own.
	 *
	 * code for the writer is at the bottom of this function.
	 *
	 * NOTE: we could be dispatched and get here before the WRITER can set the
	 * shared snapshot.  if this happens we'll have to wait around, hopefully
	 * its never for a very long time.
	 *
	 */
	if (DistributedTransactionContext == DTX_CONTEXT_QE_READER ||
		DistributedTransactionContext == DTX_CONTEXT_QE_ENTRY_DB_SINGLETON)
	{
		/* the pg_usleep() call below is in units of us (microseconds), interconnect
		 * timeout is in seconds.  Start with 1 millisecond. */
		uint64		segmate_timeout_us;
		uint64		sleep_per_check_us = 1 * 1000;
		uint64	   	total_sleep_time_us = 0;
		uint64		warning_sleep_time_us = 0;

		segmate_timeout_us = (3 * (uint64)Max(interconnect_setup_timeout, 1) * 1000* 1000) / 4;

		/*
		 * Make a copy of the distributed snapshot information; this
		 * doesn't use the shared-snapshot-slot stuff it is just
		 * making copies from the QEDtxContextInfo structure sent by
		 * the QD.
		 */
		FillInDistributedSnapshot(snapshot);

		/*
		 * If we're a cursor-reader, we get out snapshot from the
		 * writer via a tempfile in the filesystem. Otherwise it is
		 * too easy for the writer to race ahead of cursor readers.
		 */
		if (QEDtxContextInfo.cursorContext)
		{
			readSharedLocalSnapshot_forCursor(snapshot);

			return snapshot;
		}

		ereport((Debug_print_snapshot_dtm ? LOG : DEBUG5),
				(errmsg("[Distributed Snapshot #%u] *Start Reader Match* gxid = %u and currcid %d (%s)",
						QEDtxContextInfo.distributedSnapshot.distribSnapshotId,
						QEDtxContextInfo.distributedXid,
						QEDtxContextInfo.curcid,
						DtxContextToString(DistributedTransactionContext))));

		/*
		 * This is the second phase of the handshake we started in
		 * StartTransaction().  Here we get a "good" snapshot from our
		 * writer. In the process it is possible that we will change
		 * our transaction's xid (see phase-one in StartTransaction()).
		 *
		 * Here we depend on the absolute correctness of our
		 * writer-gang's info. We need the segmateSync to match *as
		 * well* as the distributed-xid since the QD may send multiple
		 * statements with the same distributed-xid/cid but
		 * *different* local-xids (MPP-3228). The dispatcher will
		 * distinguish such statements by the segmateSync.
		 *
		 * I believe that we still want the older sync mechanism ("ready" flag).
		 * since it tells the code in TransactionIdIsCurrentTransactionId() that the
		 * writer may be changing the local-xid (otherwise it would be possible for
		 * cursor reader gangs to get confused).
		 */
		for (;;)
		{
			if (QEwriterSnapshotUpToDate())
			{
				LWLockAcquire(SharedLocalSnapshotSlot->slotLock, LW_SHARED);

				/*
				 * YAY we found it.  set the contents of the
				 * SharedLocalSnapshot to this and move on.
				 */
				snapshot->xmin = SharedLocalSnapshotSlot->snapshot.xmin;
				snapshot->xmax = SharedLocalSnapshotSlot->snapshot.xmax;
				snapshot->xcnt = SharedLocalSnapshotSlot->snapshot.xcnt;

				/* We now capture our current view of the xip/combocid arrays */
				memcpy(snapshot->xip, SharedLocalSnapshotSlot->snapshot.xip, snapshot->xcnt * sizeof(TransactionId));
				memset(snapshot->xip + snapshot->xcnt, 0, (arrayP->maxProcs - snapshot->xcnt) * sizeof(TransactionId));

				snapshot->curcid = SharedLocalSnapshotSlot->snapshot.curcid;

				snapshot->subxcnt = -1;

				/* combocid */
				if (usedComboCids != SharedLocalSnapshotSlot->combocidcnt)
				{
					if (usedComboCids == 0)
					{
						MemoryContext oldCtx =  MemoryContextSwitchTo(TopTransactionContext);
						comboCids = palloc(SharedLocalSnapshotSlot->combocidcnt * sizeof(ComboCidKeyData));
						MemoryContextSwitchTo(oldCtx);
					}
					else
						repalloc(comboCids, SharedLocalSnapshotSlot->combocidcnt * sizeof(ComboCidKeyData));
				}
				memcpy(comboCids, (char *)SharedLocalSnapshotSlot->combocids, SharedLocalSnapshotSlot->combocidcnt * sizeof(ComboCidKeyData));
				usedComboCids = ((SharedLocalSnapshotSlot->combocidcnt < MaxComboCids) ? SharedLocalSnapshotSlot->combocidcnt : MaxComboCids);

				uint32 segmateSync = SharedLocalSnapshotSlot->segmateSync;
				uint32 comboCidCnt = SharedLocalSnapshotSlot->combocidcnt;

				LWLockRelease(SharedLocalSnapshotSlot->slotLock);

				ereport((Debug_print_snapshot_dtm ? LOG : DEBUG5),
						(errmsg("Reader qExec usedComboCids: %d shared %d segmateSync %d",
								usedComboCids, comboCidCnt, segmateSync)));

				SetSharedTransactionId_reader(SharedLocalSnapshotSlot->xid,
											  SharedLocalSnapshotSlot->snapshot.curcid);

				ereport((Debug_print_snapshot_dtm ? LOG : DEBUG5),
						(errmsg("Reader qExec setting shared local snapshot to: xmin: %d xmax: %d curcid: %d",
								snapshot->xmin, snapshot->xmax, snapshot->curcid)));

				ereport((Debug_print_snapshot_dtm ? LOG : DEBUG5),
						(errmsg("GetSnapshotData(): READER currentcommandid %d curcid %d segmatesync %d",
								GetCurrentCommandId(false), snapshot->curcid, segmateSync)));

				return snapshot;
			}
			else
			{
				/*
				 * didn't find it. we'll sleep for a small amount of time and
				 * then try again.
				 *
				 * TODO: is there a semaphore or something better we can do here.
				 */
				pg_usleep(sleep_per_check_us);

				CHECK_FOR_INTERRUPTS();

				warning_sleep_time_us += sleep_per_check_us;
				total_sleep_time_us += sleep_per_check_us;

				LWLockAcquire(SharedLocalSnapshotSlot->slotLock, LW_SHARED);

				if (total_sleep_time_us >= segmate_timeout_us)
				{
					ereport(ERROR,
							(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
							 errmsg("GetSnapshotData timed out waiting for Writer to set the shared snapshot."),
							 errdetail("We are waiting for the shared snapshot to have XID: %d but the value "
									   "is currently: %d."
									   " waiting for cid to be %d but is currently %d.  ready=%d."
									   "DistributedTransactionContext = %s. "
									   " Our slotindex is: %d \n"
									   "Dump of all sharedsnapshots in shmem: %s",
									   QEDtxContextInfo.distributedXid, SharedLocalSnapshotSlot->QDxid,
									   QEDtxContextInfo.curcid,
									   SharedLocalSnapshotSlot->QDcid, SharedLocalSnapshotSlot->ready,
									   DtxContextToString(DistributedTransactionContext),
									   SharedLocalSnapshotSlot->slotindex, SharedSnapshotDump())));
				}
				else if (warning_sleep_time_us > 1000 * 1000)
				{
					/*
					 * Every second issue warning.
					 */
					ereport((Debug_print_snapshot_dtm ? LOG : DEBUG5),
							(errmsg("[Distributed Snapshot #%u] *No Match* gxid %u = %u and currcid %d = %d (%s)",
									QEDtxContextInfo.distributedSnapshot.distribSnapshotId,
									QEDtxContextInfo.distributedXid,
									SharedLocalSnapshotSlot->QDxid,
									QEDtxContextInfo.curcid,
									SharedLocalSnapshotSlot->QDcid,
									DtxContextToString(DistributedTransactionContext))));


					ereport(LOG,
							(errmsg("GetSnapshotData did not find shared local snapshot information. "
									"We are waiting for the shared snapshot to have XID: %d/%u but the value "
									"is currently: %d/%u."
									" waiting for cid to be %d but is currently %d.  ready=%d."
									" Our slotindex is: %d \n"
									"DistributedTransactionContext = %s.",
									QEDtxContextInfo.distributedXid, QEDtxContextInfo.segmateSync,
									SharedLocalSnapshotSlot->QDxid, SharedLocalSnapshotSlot->segmateSync,
									QEDtxContextInfo.curcid,
									SharedLocalSnapshotSlot->QDcid,
									SharedLocalSnapshotSlot->ready,
									SharedLocalSnapshotSlot->slotindex,
									DtxContextToString(DistributedTransactionContext))));
					warning_sleep_time_us = 0;
				}

				LWLockRelease(SharedLocalSnapshotSlot->slotLock);
				/* UNDONE: Back-off from checking every millisecond... */
			}
		}
	}

	/* We must not be a reader. */
	Assert(DistributedTransactionContext != DTX_CONTEXT_QE_READER);
	Assert(DistributedTransactionContext != DTX_CONTEXT_QE_ENTRY_DB_SINGLETON);

	/*
	 * It is sufficient to get shared lock on ProcArrayLock, even if we are
	 * going to set MyProc->xmin.
	 */
	LWLockAcquire(ProcArrayLock, LW_SHARED);

	/* xmax is always latestCompletedXid + 1 */
	xmax = ShmemVariableCache->latestCompletedXid;
	Assert(TransactionIdIsNormal(xmax));
	TransactionIdAdvance(xmax);

	/* initialize xmin calculation with xmax */
	globalxmin = xmin = xmax;

	ereport((Debug_print_full_dtm ? LOG : DEBUG5),
			(errmsg("GetSnapshotData setting globalxmin and xmin to %u",
					xmin)));

	/*
	 * Get the distributed snapshot if needed and copy it into the field 
	 * called distribSnapshotWithLocalMapping in the snapshot structure.
	 *
	 * For a distributed transaction:
	 *   => The corrresponding distributed snapshot is made up of distributed
	 *      xids from the DTM that are considered in-progress will be kept in
	 *      the snapshot structure separately from any local in-progress xact.
	 *
	 *      The MVCC function XidInSnapshot is used to evaluate whether
	 *      a tuple is visible through a snapshot. Only committed xids are
	 *      given to XidInSnapshot for evaluation. XidInSnapshot will first
	 *      determine if the committed tuple is for a distributed transaction.  
	 *      If the xact is distributed it will be evaluated only against the
	 *      distributed snapshot and not the local snapshot.
	 *
	 *      Otherwise, when the committed transaction being evaluated is local,
	 *      then it will be evaluated only against the local portion of the
	 *      snapshot.
	 *
	 * For a local transaction:
	 *   => Only the local portion of the snapshot: xmin, xmax, xcnt,
	 *      in-progress (xip), etc, will be filled in.
	 *
	 *      Note that in-progress distributed transactions that have reached
	 *      this database instance and are active will be represented in the
	 *      local in-progress (xip) array with the distributed transaction's
	 *      local xid.
	 *
	 * In summary: This 2 snapshot scheme (optional distributed, required local)
	 * handles late arriving distributed transactions properly since that work
	 * is only evaluated against the distributed snapshot. And, the scheme
	 * handles local transaction work seeing distributed work properly by
	 * including distributed transactions in the local snapshot via their
	 * local xids.
	 */
	if (DistributedTransactionContext == DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE)
	{
		snapshot->haveDistribSnapshot = CreateDistributedSnapshot(&snapshot->distribSnapshotWithLocalMapping);

		ereport((Debug_print_full_dtm ? LOG : DEBUG5),
				(errmsg("Got distributed snapshot from DistributedSnapshotWithLocalXids_Create = %s",
						(snapshot->haveDistribSnapshot ? "true" : "false"))));

		/* Nice that we may have collected it, but turn it off... */
		if (Debug_disable_distributed_snapshot)
			snapshot->haveDistribSnapshot = false;
	}

	/*
	 * Spin over procArray checking xid, xmin, and subxids.  The goal is to
	 * gather all active xids, find the lowest xmin, and try to record
	 * subxids.
	 */
	for (index = 0; index < arrayP->numProcs; index++)
	{
		volatile PGPROC *proc = arrayP->procs[index];
		TransactionId xid;

#if 0 /* Upstream code not applicable to GPDB, why explained in vacuumStatement_Relation */
		/* Ignore procs running LAZY VACUUM */
		if (proc->vacuumFlags & PROC_IN_VACUUM)
			continue;
#endif

		/* Update globalxmin to be the smallest valid xmin */
		xid = proc->xmin;		/* fetch just once */
		if (TransactionIdIsNormal(xid) &&
			TransactionIdPrecedes(xid, globalxmin))
			globalxmin = xid;

		/* Fetch xid just once - see GetNewTransactionId */
		xid = proc->xid;

		/*
		 * If the transaction has been assigned an xid < xmax we add it to the
		 * snapshot, and update xmin if necessary.	There's no need to store
		 * XIDs >= xmax, since we'll treat them as running anyway.  We don't
		 * bother to examine their subxids either.
		 *
		 * We don't include our own XID (if any) in the snapshot, but we must
		 * include it into xmin.
		 */
		if (TransactionIdIsNormal(xid))
		{
			if (TransactionIdFollowsOrEquals(xid, xmax))
				continue;
			if (proc != MyProc)
				snapshot->xip[count++] = xid;
			if (TransactionIdPrecedes(xid, xmin))
				xmin = xid;
		}

		/*
		 * Save subtransaction XIDs if possible (if we've already overflowed,
		 * there's no point).  Note that the subxact XIDs must be later than
		 * their parent, so no need to check them against xmin.  We could
		 * filter against xmax, but it seems better not to do that much work
		 * while holding the ProcArrayLock.
		 *
		 * The other backend can add more subxids concurrently, but cannot
		 * remove any.	Hence it's important to fetch nxids just once. Should
		 * be safe to use memcpy, though.  (We needn't worry about missing any
		 * xids added concurrently, because they must postdate xmax.)
		 *
		 * Again, our own XIDs are not included in the snapshot.
		 */
		if (subcount >= 0 && proc != MyProc)
		{
			if (proc->subxids.overflowed)
				subcount = -1;	/* overflowed */
			else
			{
				int			nxids = proc->subxids.nxids;

				if (nxids > 0)
				{
					memcpy(snapshot->subxip + subcount,
						   (void *) proc->subxids.xids,
						   nxids * sizeof(TransactionId));
					subcount += nxids;
				}
			}
		}
	}

	if (!TransactionIdIsValid(MyProc->xmin))
	{
		/* Not that these values are not set atomically. However,
		 * each of these assignments is itself assumed to be atomic. */
		MyProc->xmin = TransactionXmin = xmin;
	}
	if (IsXactIsoLevelSerializable)
	{
		MyProc->serializableIsoLevel = true;

		ereport((Debug_print_snapshot_dtm ? LOG : DEBUG3),
				(errmsg("Got serializable snapshot: database %d, pid %d, xid %d, xmin %d",
						MyProc->databaseId, MyProc->pid, MyProc->xid, MyProc->xmin)));
	}

	LWLockRelease(ProcArrayLock);

	/*
	 * Fill in the distributed snapshot information we received from the the QD.
	 * Unless we are the QD, in which case we already created a new distributed
	 * snapshot above.
	 *
	 * (We do this after releasing ProcArrayLock, reduce contention.)
	 */
	if (DistributedTransactionContext != DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE)
		FillInDistributedSnapshot(snapshot);

	/*
	 * Update globalxmin to include actual process xids.  This is a slightly
	 * different way of computing it than GetOldestXmin uses, but should give
	 * the same result.
	 */
	if (TransactionIdPrecedes(xmin, globalxmin))
		globalxmin = xmin;

	/* Update global variables too */
	RecentGlobalXmin = globalxmin;
	RecentXmin = xmin;

	snapshot->xmin = xmin;
	snapshot->xmax = xmax;
	snapshot->xcnt = count;
	snapshot->subxcnt = subcount;

	snapshot->curcid = GetCurrentCommandId(false);

	/*
	 * This is a new snapshot, so set both refcounts are zero, and mark it
	 * as not copied in persistent memory.
	 */
	snapshot->active_count = 0;
	snapshot->regd_count = 0;
	snapshot->copied = false;

	/*
	 * MPP Addition. If we are the chief then we'll save our local snapshot
	 * into the shared snapshot. Note: we need to use the shared local
	 * snapshot for the "Local Implicit using Distributed Snapshot" case, too.
	 */
	
	if ((DistributedTransactionContext == DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER ||
		 DistributedTransactionContext == DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER ||
		 DistributedTransactionContext == DTX_CONTEXT_QE_AUTO_COMMIT_IMPLICIT) &&
		SharedLocalSnapshotSlot != NULL)
	{
		updateSharedLocalSnapshot(&QEDtxContextInfo, snapshot, "GetSnapshotData");
	}

	ereport((Debug_print_snapshot_dtm ? LOG : DEBUG5),
			(errmsg("GetSnapshotData(): WRITER currentcommandid %d curcid %d segmatesync %d",
					GetCurrentCommandId(false), snapshot->curcid, QEDtxContextInfo.segmateSync)));

	return snapshot;
}

/*
 * GetVirtualXIDsDelayingChkpt -- Get the VXIDs of transactions that are
 * delaying checkpoint because they have critical actions in progress.
 *
 * Constructs an array of VXIDs of transactions that are currently in commit
 * critical sections, as shown by having inCommit set in their PGXACT.
 *
 * Returns a palloc'd array that should be freed by the caller.
 * *nvxids is the number of valid entries.
 *
 * Note that because backends set or clear inCommit without holding any lock,
 * the result is somewhat indeterminate, but we don't really care.  Even in
 * a multiprocessor with delayed writes to shared memory, it should be certain
 * that setting of inCommit will propagate to shared memory when the backend
 * takes a lock, so we cannot fail to see a virtual xact as inCommit if
 * it's already inserted its commit record.  Whether it takes a little while
 * for clearing of inCommit to propagate is unimportant for correctness.
 */
VirtualTransactionId *
GetVirtualXIDsDelayingChkpt(int *nvxids)
{
	VirtualTransactionId *vxids;
	ProcArrayStruct *arrayP = procArray;
	int			count = 0;
	int			index;

	/* allocate what's certainly enough result space */
	vxids = (VirtualTransactionId *)
		palloc(sizeof(VirtualTransactionId) * arrayP->maxProcs);

	LWLockAcquire(ProcArrayLock, LW_SHARED);

	for (index = 0; index < arrayP->numProcs; index++)
	{
		volatile PGPROC *proc = arrayP->procs[index];

		if (proc->inCommit)
		{
			VirtualTransactionId vxid;

			GET_VXID_FROM_PGPROC(vxid, *proc);
			if (VirtualTransactionIdIsValid(vxid))
				vxids[count++] = vxid;
		}
	}

	LWLockRelease(ProcArrayLock);

	*nvxids = count;
	return vxids;
}

/*
 * HaveVirtualXIDsDelayingChkpt -- Are any of the specified VXIDs delaying?
 *
 * This is used with the results of GetVirtualXIDsDelayingChkpt to see if any
 * of the specified VXIDs are still in critical sections of code.
 *
 * Note: this is O(N^2) in the number of vxacts that are/were delaying, but
 * those numbers should be small enough for it not to be a problem.
 */
bool
HaveVirtualXIDsDelayingChkpt(VirtualTransactionId *vxids, int nvxids)
{
	bool		result = false;
	ProcArrayStruct *arrayP = procArray;
	int			index;

	LWLockAcquire(ProcArrayLock, LW_SHARED);

	for (index = 0; index < arrayP->numProcs; index++)
	{
		volatile PGPROC *proc = arrayP->procs[index];
		VirtualTransactionId vxid;

		GET_VXID_FROM_PGPROC(vxid, *proc);

		if (proc->inCommit && VirtualTransactionIdIsValid(vxid))
		{
			int			i;

			for (i = 0; i < nvxids; i++)
			{
				if (VirtualTransactionIdEquals(vxid, vxids[i]))
				{
					result = true;
					break;
				}
			}
			if (result)
				break;
		}
	}

	LWLockRelease(ProcArrayLock);

	return result;
}

/*
 * MPP: Special code to update the command id in the SharedLocalSnapshot
 * when we are in SERIALIZABLE isolation mode.
 */
void
UpdateSerializableCommandId(CommandId curcid)
{
	if ((DistributedTransactionContext == DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER ||
		 DistributedTransactionContext == DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER) &&
		 SharedLocalSnapshotSlot != NULL &&
		 FirstSnapshotSet)
	{
		int combocidSize;

		LWLockAcquire(SharedLocalSnapshotSlot->slotLock, LW_EXCLUSIVE);

		if (SharedLocalSnapshotSlot->QDxid != QEDtxContextInfo.distributedXid)
		{
			ereport((Debug_print_snapshot_dtm ? LOG : DEBUG5),
					(errmsg("[Distributed Snapshot #%u] *Can't Update Serializable Command Id* QDxid = %u (gxid = %u, '%s')",
							QEDtxContextInfo.distributedSnapshot.distribSnapshotId,
							SharedLocalSnapshotSlot->QDxid,
							getDistributedTransactionId(),
							DtxContextToString(DistributedTransactionContext))));
			LWLockRelease(SharedLocalSnapshotSlot->slotLock);
			return;
		}

		ereport((Debug_print_snapshot_dtm ? LOG : DEBUG5),
				(errmsg("[Distributed Snapshot #%u] *Update Serializable Command Id* segment currcid = %d, QDcid = %d, TransactionSnapshot currcid = %d, Shared currcid = %d (gxid = %u, '%s')",
						QEDtxContextInfo.distributedSnapshot.distribSnapshotId,
						QEDtxContextInfo.curcid,
						SharedLocalSnapshotSlot->QDcid,
						curcid,
						SharedLocalSnapshotSlot->snapshot.curcid,
						getDistributedTransactionId(),
						DtxContextToString(DistributedTransactionContext))));

		ereport((Debug_print_snapshot_dtm ? LOG : DEBUG5),
				(errmsg("serializable writer updating combocid: used combocids %d shared %d",
						usedComboCids, SharedLocalSnapshotSlot->combocidcnt)));

		combocidSize = ((usedComboCids < MaxComboCids) ? usedComboCids : MaxComboCids );

		SharedLocalSnapshotSlot->combocidcnt = combocidSize;	
		memcpy((void *)SharedLocalSnapshotSlot->combocids, comboCids,
			   combocidSize * sizeof(ComboCidKeyData));

		SharedLocalSnapshotSlot->snapshot.curcid = curcid;
		SharedLocalSnapshotSlot->QDcid = QEDtxContextInfo.curcid;
		SharedLocalSnapshotSlot->segmateSync = QEDtxContextInfo.segmateSync;

		LWLockRelease(SharedLocalSnapshotSlot->slotLock);
	}
}

/*
 * BackendPidGetProc -- get a backend's PGPROC given its PID
 *
 * Returns NULL if not found.  Note that it is up to the caller to be
 * sure that the question remains meaningful for long enough for the
 * answer to be used ...
 */
PGPROC *
BackendPidGetProc(int pid)
{
	PGPROC	   *result = NULL;
	ProcArrayStruct *arrayP = procArray;
	int			index;

	if (pid == 0)				/* never match dummy PGPROCs */
		return NULL;

	LWLockAcquire(ProcArrayLock, LW_SHARED);

	for (index = 0; index < arrayP->numProcs; index++)
	{
		PGPROC	   *proc = arrayP->procs[index];

		if (proc->pid == pid)
		{
			result = proc;
			break;
		}
	}

	LWLockRelease(ProcArrayLock);

	return result;
}

/*
 * BackendXidGetPid -- get a backend's pid given its XID
 *
 * Returns 0 if not found or it's a prepared transaction.  Note that
 * it is up to the caller to be sure that the question remains
 * meaningful for long enough for the answer to be used ...
 *
 * Only main transaction Ids are considered.  This function is mainly
 * useful for determining what backend owns a lock.
 *
 * Beware that not every xact has an XID assigned.	However, as long as you
 * only call this using an XID found on disk, you're safe.
 */
int
BackendXidGetPid(TransactionId xid)
{
	int			result = 0;
	ProcArrayStruct *arrayP = procArray;
	int			index;

	if (xid == InvalidTransactionId)	/* never match invalid xid */
		return 0;

	LWLockAcquire(ProcArrayLock, LW_SHARED);

	for (index = 0; index < arrayP->numProcs; index++)
	{
		volatile PGPROC *proc = arrayP->procs[index];

		if (proc->xid == xid)
		{
			result = proc->pid;
			break;
		}
	}

	LWLockRelease(ProcArrayLock);

	return result;
}

/*
 * IsBackendPid -- is a given pid a running backend
 */
bool
IsBackendPid(int pid)
{
	return (BackendPidGetProc(pid) != NULL);
}


/*
 * GetCurrentVirtualXIDs -- returns an array of currently active VXIDs.
 *
 * The array is palloc'd and is terminated with an invalid VXID.
 *
 * If limitXmin is not InvalidTransactionId, we skip any backends
 * with xmin >= limitXmin.	If allDbs is false, we skip backends attached
 * to other databases.  If excludeVacuum isn't zero, we skip processes for
 * which (excludeVacuum & vacuumFlags) is not zero.  Also, our own process
 * is always skipped.
 */
VirtualTransactionId *
GetCurrentVirtualXIDs(TransactionId limitXmin, bool allDbs, int excludeVacuum)
{
	VirtualTransactionId *vxids;
	ProcArrayStruct *arrayP = procArray;
	int			count = 0;
	int			index;

	/* allocate result space with room for a terminator */
	vxids = (VirtualTransactionId *)
		palloc(sizeof(VirtualTransactionId) * (arrayP->maxProcs + 1));

	LWLockAcquire(ProcArrayLock, LW_SHARED);

	for (index = 0; index < arrayP->numProcs; index++)
	{
		volatile PGPROC *proc = arrayP->procs[index];

		if (proc == MyProc)
			continue;

		if (excludeVacuum & proc->vacuumFlags)
			continue;

		if (allDbs || proc->databaseId == MyDatabaseId)
		{
			/* Fetch xmin just once - might change on us? */
			TransactionId pxmin = proc->xmin;

			/*
			 * Note that InvalidTransactionId precedes all other XIDs, so a
			 * proc that hasn't set xmin yet will always be included.
			 */
			if (!TransactionIdIsValid(limitXmin) ||
				TransactionIdPrecedes(pxmin, limitXmin))
			{
				VirtualTransactionId vxid;

				GET_VXID_FROM_PGPROC(vxid, *proc);
				if (VirtualTransactionIdIsValid(vxid))
					vxids[count++] = vxid;
			}
		}
	}

	LWLockRelease(ProcArrayLock);

	/* add the terminator */
	vxids[count].backendId = InvalidBackendId;
	vxids[count].localTransactionId = InvalidLocalTransactionId;

	return vxids;
}


/*
 * CountActiveBackends --- count backends (other than myself) that are in
 *		active transactions.  This is used as a heuristic to decide if
 *		a pre-XLOG-flush delay is worthwhile during commit.
 *
 * Do not count backends that are blocked waiting for locks, since they are
 * not going to get to run until someone else commits.
 */
int
CountActiveBackends(void)
{
	ProcArrayStruct *arrayP = procArray;
	int			count = 0;
	int			index;

	/*
	 * Note: for speed, we don't acquire ProcArrayLock.  This is a little bit
	 * bogus, but since we are only testing fields for zero or nonzero, it
	 * should be OK.  The result is only used for heuristic purposes anyway...
	 */
	for (index = 0; index < arrayP->numProcs; index++)
	{
		volatile PGPROC *proc = arrayP->procs[index];

		/*
		 * Since we're not holding a lock, need to check that the pointer is
		 * valid. Someone holding the lock could have incremented numProcs
		 * already, but not yet inserted a valid pointer to the array.
		 *
		 * If someone just decremented numProcs, 'proc' could also point to a
		 * PGPROC entry that's no longer in the array. It still points to a
		 * PGPROC struct, though, because freed PGPPROC entries just go to
		 * the free list and are recycled. Its contents are nonsense in that
		 * case, but that's acceptable for this function.
		 */
		if (proc == NULL)
			continue;

		if (proc == MyProc)
			continue;			/* do not count myself */
		if (proc->pid == 0)
			continue;			/* do not count prepared xacts */
		if (proc->xid == InvalidTransactionId)
			continue;			/* do not count if no XID assigned */
		if (proc->waitLock != NULL)
			continue;			/* do not count if blocked on a lock */
		count++;
	}

	return count;
}

/*
 * CountDBBackends --- count backends that are using specified database
 */
int
CountDBBackends(Oid databaseid)
{
	ProcArrayStruct *arrayP = procArray;
	int			count = 0;
	int			index;

	LWLockAcquire(ProcArrayLock, LW_SHARED);

	for (index = 0; index < arrayP->numProcs; index++)
	{
		volatile PGPROC *proc = arrayP->procs[index];

		if (proc->pid == 0)
			continue;			/* do not count prepared xacts */
		if (proc->databaseId == databaseid)
			count++;
	}

	LWLockRelease(ProcArrayLock);

	return count;
}

/*
 * CountUserBackends --- count backends that are used by specified user
 */
int
CountUserBackends(Oid roleid)
{
	ProcArrayStruct *arrayP = procArray;
	int			count = 0;
	int			index;

	LWLockAcquire(ProcArrayLock, LW_SHARED);

	for (index = 0; index < arrayP->numProcs; index++)
	{
		volatile PGPROC *proc = arrayP->procs[index];

		if (proc->pid == 0)
			continue;			/* do not count prepared xacts */
		if (proc->roleId == roleid)
			count++;
	}

	LWLockRelease(ProcArrayLock);

	return count;
}

/*
 * CountOtherDBBackends -- check for other backends running in the given DB
 *
 * If there are other backends in the DB, we will wait a maximum of 5 seconds
 * for them to exit.  Autovacuum backends are encouraged to exit early by
 * sending them SIGTERM, but normal user backends are just waited for.
 *
 * The current backend is always ignored; it is caller's responsibility to
 * check whether the current backend uses the given DB, if it's important.
 *
 * Returns TRUE if there are (still) other backends in the DB, FALSE if not.
 * Also, *nbackends and *nprepared are set to the number of other backends
 * and prepared transactions in the DB, respectively.
 *
 * This function is used to interlock DROP DATABASE and related commands
 * against there being any active backends in the target DB --- dropping the
 * DB while active backends remain would be a Bad Thing.  Note that we cannot
 * detect here the possibility of a newly-started backend that is trying to
 * connect to the doomed database, so additional interlocking is needed during
 * backend startup.  The caller should normally hold an exclusive lock on the
 * target DB before calling this, which is one reason we mustn't wait
 * indefinitely.
 */
bool
CountOtherDBBackends(Oid databaseId, int *nbackends, int *nprepared)
{
	ProcArrayStruct *arrayP = procArray;
#define MAXAUTOVACPIDS  10		/* max autovacs to SIGTERM per iteration */
	int			autovac_pids[MAXAUTOVACPIDS];
	int			tries;

	/* 50 tries with 100ms sleep between tries makes 5 sec total wait */
	for (tries = 0; tries < 50; tries++)
	{
		int			nautovacs = 0;
		bool		found = false;
		int			index;

		CHECK_FOR_INTERRUPTS();

		*nbackends = *nprepared = 0;

		LWLockAcquire(ProcArrayLock, LW_SHARED);

		for (index = 0; index < arrayP->numProcs; index++)
		{
			volatile PGPROC *proc = arrayP->procs[index];

			if (proc->databaseId != databaseId)
				continue;
			if (proc == MyProc)
				continue;

			found = true;

			if (proc->pid == 0)
				(*nprepared)++;
			else
			{
				(*nbackends)++;
				if ((proc->vacuumFlags & PROC_IS_AUTOVACUUM) &&
					nautovacs < MAXAUTOVACPIDS)
					autovac_pids[nautovacs++] = proc->pid;
			}
		}

		LWLockRelease(ProcArrayLock);

		if (!found)
			return false;		/* no conflicting backends, so done */

		/*
		 * Send SIGTERM to any conflicting autovacuums before sleeping.
		 * We postpone this step until after the loop because we don't
		 * want to hold ProcArrayLock while issuing kill().
		 * We have no idea what might block kill() inside the kernel...
		 */
		for (index = 0; index < nautovacs; index++)
			(void) kill(autovac_pids[index], SIGTERM);	/* ignore any error */

		/* sleep, then try again */
		pg_usleep(100 * 1000L); /* 100ms */
	}

	return true;				/* timed out, still conflicts */
}


#define XidCacheRemove(i) \
	do { \
		MyProc->subxids.xids[i] = MyProc->subxids.xids[MyProc->subxids.nxids - 1]; \
		MyProc->subxids.nxids--; \
	} while (0)

/*
 * XidCacheRemoveRunningXids
 *
 * Remove a bunch of TransactionIds from the list of known-running
 * subtransactions for my backend.	Both the specified xid and those in
 * the xids[] array (of length nxids) are removed from the subxids cache.
 * latestXid must be the latest XID among the group.
 */
void
XidCacheRemoveRunningXids(TransactionId xid,
						  int nxids, const TransactionId *xids,
						  TransactionId latestXid)
{
	int			i,
				j;

	Assert(TransactionIdIsValid(xid));

	/*
	 * We must hold ProcArrayLock exclusively in order to remove transactions
	 * from the PGPROC array.  (See src/backend/access/transam/README.)  It's
	 * possible this could be relaxed since we know this routine is only used
	 * to abort subtransactions, but pending closer analysis we'd best be
	 * conservative.
	 */
	LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

	/*
	 * Under normal circumstances xid and xids[] will be in increasing order,
	 * as will be the entries in subxids.  Scan backwards to avoid O(N^2)
	 * behavior when removing a lot of xids.
	 */
	for (i = nxids - 1; i >= 0; i--)
	{
		TransactionId anxid = xids[i];

		for (j = MyProc->subxids.nxids - 1; j >= 0; j--)
		{
			if (TransactionIdEquals(MyProc->subxids.xids[j], anxid))
			{
				XidCacheRemove(j);
				break;
			}
		}

		/*
		 * Ordinarily we should have found it, unless the cache has
		 * overflowed. However it's also possible for this routine to be
		 * invoked multiple times for the same subtransaction, in case of an
		 * error during AbortSubTransaction.  So instead of Assert, emit a
		 * debug warning.
		 */
		if (j < 0 && !MyProc->subxids.overflowed)
			elog(WARNING, "did not find subXID %u in MyProc", anxid);
	}

	for (j = MyProc->subxids.nxids - 1; j >= 0; j--)
	{
		if (TransactionIdEquals(MyProc->subxids.xids[j], xid))
		{
			XidCacheRemove(j);
			break;
		}
	}
	/* Ordinarily we should have found it, unless the cache has overflowed */
	if (j < 0 && !MyProc->subxids.overflowed)
		elog(WARNING, "did not find subXID %u in MyProc", xid);

	/* Also advance global latestCompletedXid while holding the lock */
	if (TransactionIdPrecedes(ShmemVariableCache->latestCompletedXid,
							  latestXid))
		ShmemVariableCache->latestCompletedXid = latestXid;

	LWLockRelease(ProcArrayLock);
}

#ifdef XIDCACHE_DEBUG

/*
 * Print stats about effectiveness of XID cache
 */
static void
DisplayXidCache(void)
{
	fprintf(stderr,
			"XidCache: xmin: %ld, known: %ld, myxact: %ld, latest: %ld, mainxid: %ld, childxid: %ld, nooflo: %ld, slow: %ld\n",
			xc_by_recent_xmin,
			xc_by_known_xact,
			xc_by_my_xact,
			xc_by_latest_xid,
			xc_by_main_xid,
			xc_by_child_xid,
			xc_no_overflow,
			xc_slow_answer);
}

#endif   /* XIDCACHE_DEBUG */

PGPROC *
FindProcByGpSessionId(long gp_session_id)
{
	/* Find the guy who should manage our locks */
	ProcArrayStruct *arrayP = procArray;
	int			index;

	Assert(gp_session_id > 0);
		
	LWLockAcquire(ProcArrayLock, LW_SHARED);

	for (index = 0; index < arrayP->numProcs; index++)
	{
		PGPROC	   *proc = arrayP->procs[index];
			
		if (proc->pid == MyProc->pid)
			continue;
				
		if (!proc->mppIsWriter)
			continue;
				
		if (proc->mppSessionId == gp_session_id)
		{
			LWLockRelease(ProcArrayLock);
			return proc;
		}
	}
		
	LWLockRelease(ProcArrayLock);
	return NULL;
}

/*
 * FindAndSignalProcess
 *     Find the PGPROC entry in procArray which contains the given sessionId and commandId,
 *     and send the corresponding process an interrupt signal.
 *
 * This function returns false if not such an entry found in procArray or the interrupt
 * signal can not be sent to the process.
 */
bool
FindAndSignalProcess(int sessionId, int commandId)
{
	Assert(sessionId > 0 && commandId > 0);
	bool queryCancelled = false;
	int pid = 0;

	LWLockAcquire(ProcArrayLock, LW_SHARED);

	for (int index = 0; index < procArray->numProcs; index++)
	{
		PGPROC *proc = procArray->procs[index];
		
		if (proc->mppSessionId == sessionId &&
			proc->queryCommandId == commandId)
		{
			/* If we have setsid(), signal the backend's whole process group */
#ifdef HAVE_SETSID
			if (kill(-proc->pid, SIGINT) == 0)
#else
			if (kill(proc->pid, SIGINT) == 0)
#endif
			{
				pid = proc->pid;
				queryCancelled = true;
			}
			
			break;
		}
	}

	LWLockRelease(ProcArrayLock);

	if (gp_cancel_query_print_log && queryCancelled)
	{
		elog(NOTICE, "sent an interrupt to process %d", pid);
	}

	return queryCancelled;
}
