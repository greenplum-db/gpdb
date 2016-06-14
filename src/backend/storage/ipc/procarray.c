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
 *	  $PostgreSQL: pgsql/src/backend/storage/ipc/procarray.c,v 1.21 2007/01/16 13:28:56 alvherre Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <signal.h>

#include "access/distributedlog.h"
#include "access/subtrans.h"
#include "access/transam.h"
#include "access/xact.h"
#include "utils/tqual.h"
#include "access/twophase.h"
#include "miscadmin.h"
#include "storage/procarray.h"
#include "utils/combocid.h"
#include "cdb/cdbtm.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/faultinjector.h"
#include "utils/sharedsnapshot.h"

#include "access/xact.h"		/* setting the shared xid */

#include "cdb/cdbvars.h"

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
static long xc_by_main_xid = 0;
static long xc_by_child_xid = 0;
static long xc_slow_answer = 0;

#define xc_by_recent_xmin_inc()		(xc_by_recent_xmin++)
#define xc_by_main_xid_inc()		(xc_by_main_xid++)
#define xc_by_child_xid_inc()		(xc_by_child_xid++)
#define xc_slow_answer_inc()		(xc_slow_answer++)

static void DisplayXidCache(void);

#else							/* !XIDCACHE_DEBUG */

#define xc_by_recent_xmin_inc()		((void) 0)
#define xc_by_main_xid_inc()		((void) 0)
#define xc_by_child_xid_inc()		((void) 0)
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

#ifdef FAULT_INJECTOR
	FaultInjector_InjectFaultIfSet(
		ProcArray_Add,
		DDLNotSpecified,
		"", // databaseName
		""); // tableName
#endif

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
 */
void
ProcArrayRemove(PGPROC *proc, bool forPrepare, bool isCommit)
{
	ProcArrayStruct *arrayP = procArray;
	int			index;

#ifdef XIDCACHE_DEBUG
	/* dump stats at backend shutdown, but not prepared-xact end */
	if (proc->pid != 0)
		DisplayXidCache();
#endif

	LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

	for (index = 0; index < arrayP->numProcs; index++)
	{
		if (arrayP->procs[index] == proc)
		{
			arrayP->procs[index] = arrayP->procs[arrayP->numProcs - 1];
			arrayP->numProcs--;

			if (forPrepare)
			{
				LocalDistribXact_ChangeStateUnderLock(
												proc->xid,
												&proc->localDistribXactRef,
												(isCommit ?
													LOCALDISTRIBXACT_STATE_COMMITPREPARED:
													LOCALDISTRIBXACT_STATE_ABORTPREPARED));

				LocalDistribXactRef_ReleaseUnderLock(
												&proc->localDistribXactRef);
			}
			LWLockRelease(ProcArrayLock);
			return;
		}
	}

	/* Ooops */
	LWLockRelease(ProcArrayLock);

	elog(LOG, "failed to find proc %p in ProcArray", proc);
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
	bool		result = false;
	ProcArrayStruct *arrayP = procArray;
	int			i,
				j;
	int			nxids = 0;
	TransactionId *xids;
	TransactionId topxid;
	bool		locked;

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

	/* Get workspace to remember main XIDs in */
	xids = (TransactionId *) palloc(sizeof(TransactionId) * arrayP->maxProcs);

	LWLockAcquire(ProcArrayLock, LW_SHARED);
	locked = true;

	for (i = 0; i < arrayP->numProcs; i++)
	{
		volatile PGPROC *proc = arrayP->procs[i];

		/* Fetch xid just once - see GetNewTransactionId */
		TransactionId pxid = proc->xid;

		if (!TransactionIdIsValid(pxid))
			continue;

		/*
		 * Step 1: check the main Xid
		 */
		if (TransactionIdEquals(pxid, xid))
		{
			xc_by_main_xid_inc();
			result = true;
			goto result_known;
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
				xc_by_child_xid_inc();
				result = true;
				goto result_known;
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
	locked = false;

	/*
	 * If none of the relevant caches overflowed, we know the Xid is not
	 * running without looking at pg_subtrans.
	 */
	if (nxids == 0)
		goto result_known;

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
		goto result_known;

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
			{
				result = true;
				break;
			}
		}
	}

result_known:
	if (locked)
		LWLockRelease(ProcArrayLock);

	pfree(xids);

	return result;
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
				elog((Debug_print_snapshot_dtm ? LOG : DEBUG3), "Found drop transaction: "
					"database %d, pid %d, xid %d, xmin %d",
					proc->databaseId, proc->pid, proc->xid, proc->xmin);
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
				elog((Debug_print_snapshot_dtm ? LOG : DEBUG3), "Found serializable transaction: "
					"database %d, pid %d, xid %d, xmin %d",
					proc->databaseId, proc->pid, proc->xid, proc->xmin);
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
 * Note: we include all currently running xids in the set of considered xids.
 * This ensures that if a just-started xact has not yet set its snapshot,
 * when it does set the snapshot it cannot set xmin less than what we compute.
 * See notes in src/backend/access/transam/README.
 */
TransactionId
GetOldestXmin(bool allDbs)
{
	ProcArrayStruct *arrayP = procArray;
	TransactionId result;
	int			index;

	/*
	 * Normally we start the min() calculation with our own XID.  But if
	 * called by checkpointer, we will not be inside a transaction, so use
	 * next XID as starting point for min() calculation.  (Note that if there
	 * are no xacts running at all, that will be the subtrans truncation
	 * point!)
	 */
	if (IsTransactionState())
		result = GetTopTransactionId();
	else
		result = ReadNewTransactionId();

	LWLockAcquire(ProcArrayLock, LW_SHARED);

	for (index = 0; index < arrayP->numProcs; index++)
	{
		volatile PGPROC *proc = arrayP->procs[index];

		if (allDbs || proc->databaseId == MyDatabaseId)
		{
			/* Fetch xid just once - see GetNewTransactionId */
			TransactionId xid = proc->xid;

			if (TransactionIdIsNormal(xid))
			{
				/* First consider the transaction own's Xid */
				if (TransactionIdPrecedes(xid, result))
					result = xid;

				/*
				 * Also consider the transaction's Xmin, if set.
				 *
				 * We must check both Xid and Xmin because there is a window
				 * where an xact's Xid is set but Xmin isn't yet.
				 */
				xid = proc->xmin;
				if (TransactionIdIsNormal(xid))
					if (TransactionIdPrecedes(xid, result))
						result = xid;
			}
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

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "updateSharedLocalSnapshot for DistributedTransactionContext = '%s' passed local snapshot (xmin: %u xmax: %u xcnt: %u) curcid: %d", 
		 DtxContextToString(DistributedTransactionContext),
		 snapshot->xmin,
		 snapshot->xmax,
		 snapshot->xcnt,
		 snapshot->curcid);
			 
	SharedLocalSnapshotSlot->snapshot.xmin = snapshot->xmin;
	SharedLocalSnapshotSlot->snapshot.xmax = snapshot->xmax;
	SharedLocalSnapshotSlot->snapshot.xcnt = snapshot->xcnt;

	/* UNDONE: Are xip and subxids broken in the SharedLocalSnapshotSlot? */

	/*
	 * Copy all active subtransctions to shared snapshot.
	 *
	 */
	UpdateSubtransactionsInSharedSnapshot(dtxContextInfo->distributedXid);

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "updateSharedLocalSnapshot subxid cnt: total %u, in memory %u",
		 SharedLocalSnapshotSlot->total_subcnt,
		 SharedLocalSnapshotSlot->inmemory_subcnt);

	if (snapshot->xcnt > 0)
	{
		Assert(snapshot->xip != NULL);

		elog((Debug_print_full_dtm ? LOG : DEBUG5),"updateSharedLocalSnapshot count of in-doubt ids %u", SharedLocalSnapshotSlot->snapshot.xcnt);

		memcpy(SharedLocalSnapshotSlot->snapshot.xip, snapshot->xip, snapshot->xcnt * sizeof(TransactionId));
	}
	
	/* combocid stuff */
	combocidSize = ((usedComboCids < MaxComboCids) ? usedComboCids : MaxComboCids );

	SharedLocalSnapshotSlot->combocidcnt = combocidSize;	
	memcpy((void *)SharedLocalSnapshotSlot->combocids, comboCids,
		   combocidSize * sizeof(ComboCidKeyData));

	SharedLocalSnapshotSlot->snapshot.curcid = snapshot->curcid;

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "updateSharedLocalSnapshot: combocidsize is now %d max %d segmateSync %d->%d",
		 combocidSize, MaxComboCids, SharedLocalSnapshotSlot->segmateSync, dtxContextInfo->segmateSync);

	SetSharedTransactionId();
	
	SharedLocalSnapshotSlot->QDcid = dtxContextInfo->curcid;
	SharedLocalSnapshotSlot->QDxid = dtxContextInfo->distributedXid;
		
	SharedLocalSnapshotSlot->ready = true;

	SharedLocalSnapshotSlot->segmateSync = dtxContextInfo->segmateSync;

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "updateSharedLocalSnapshot for DistributedTransactionContext = '%s' setting shared local snapshot xid = %u (xmin: %u xmax: %u xcnt: %u) curcid: %d, QDxid = %u, QDcid = %u", 
		 DtxContextToString(DistributedTransactionContext),
		 SharedLocalSnapshotSlot->xid,
		 SharedLocalSnapshotSlot->snapshot.xmin,
		 SharedLocalSnapshotSlot->snapshot.xmax,
		 SharedLocalSnapshotSlot->snapshot.xcnt,
		 SharedLocalSnapshotSlot->snapshot.curcid,
		 SharedLocalSnapshotSlot->QDxid,
		 SharedLocalSnapshotSlot->QDcid);

	elog((Debug_print_snapshot_dtm ? LOG : DEBUG5), "[Distributed Snapshot #%u] *Writer Set Shared* gxid %u, currcid %d (gxid = %u, slot #%d, '%s', '%s')", 
		QEDtxContextInfo.distributedSnapshot.header.distribSnapshotId,
		SharedLocalSnapshotSlot->QDxid,
		SharedLocalSnapshotSlot->QDcid,
		getDistributedTransactionId(),
		SharedLocalSnapshotSlot->slotid,
		debugCaller,
		DtxContextToString(DistributedTransactionContext));
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
		if (QEDtxContextInfo.distributedSnapshot.header.distribSnapshotId != 0)
			return QEDtxContextInfo.distributedSnapshot.header.maxCount;
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

static void
FillInDistributedSnapshot(Snapshot snapshot)
{
	elog((Debug_print_full_dtm ? LOG : DEBUG5),
	     "FillInDistributedSnapshot DTX Context = '%s'", 
	     DtxContextToString(DistributedTransactionContext));
	
	switch (DistributedTransactionContext)
	{
	case DTX_CONTEXT_LOCAL_ONLY:
	case DTX_CONTEXT_QD_RETRY_PHASE_2:
	case DTX_CONTEXT_QE_FINISH_PREPARED:
		/*
		 * No distributed snapshot.
		 */
		snapshot->haveDistribSnapshot = false;
		snapshot->distribSnapshotWithLocalMapping.header.count = 0;
		break;

	case DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE:
		/*
		 * Create distributed snapshot since we are the master (QD).
		 */
		Assert(snapshot->distribSnapshotWithLocalMapping.inProgressEntryArray != NULL);
		snapshot->haveDistribSnapshot = 
						createDtxSnapshot(
									&snapshot->distribSnapshotWithLocalMapping);
		
		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "Got distributed snapshot from DistributedSnapshotWithLocalXids_Create = %s",
			 (snapshot->haveDistribSnapshot ? "true" : "false"));
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
			DistributedSnapshotWithLocalMapping *dslm = &snapshot->distribSnapshotWithLocalMapping;

			if (ds->header.distribSnapshotId != 0)
			{
				int count;
				int i;
				
				if (dslm->header.maxCount < ds->header.count)
					elog(ERROR, "Distributed snapshot in-progress array too long");

				snapshot->haveDistribSnapshot = true;

				dslm->header.distribTransactionTimeStamp = ds->header.distribTransactionTimeStamp;
				dslm->header.distribSnapshotId = ds->header.distribSnapshotId;
				
				dslm->header.xmin = ds->header.xmin;
				dslm->header.xmax = ds->header.xmax;
				dslm->header.count = ds->header.count;
				/* Do not copy maxCount. */

				count = ds->header.count;
				
				for (i = 0; i < count; i++)
				{
					dslm->inProgressEntryArray[i].distribXid =
											ds->inProgressXidArray[i];

					/* UNDONE: Lookup in distributed cache. */
					dslm->inProgressEntryArray[i].localXid =
											InvalidTransactionId;
				}
			}
			else
			{
				snapshot->haveDistribSnapshot = false;
				snapshot->distribSnapshotWithLocalMapping.header.count = 0;
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
	{
		snapshot->haveDistribSnapshot = false;
	}
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

	if (QEDtxContextInfo.distributedXid == SharedLocalSnapshotSlot->QDxid &&
		QEDtxContextInfo.curcid == SharedLocalSnapshotSlot->QDcid &&
		QEDtxContextInfo.segmateSync == SharedLocalSnapshotSlot->segmateSync &&
		SharedLocalSnapshotSlot->ready)
	{
		return true;
	}

	return false;
}

/*----------
 * GetSnapshotData -- returns information about running transactions.
 *
 * The returned snapshot includes xmin (lowest still-running xact ID),
 * xmax (next xact ID to be assigned), and a list of running xact IDs
 * in the range xmin <= xid < xmax.  It is used as follows:
 *		All xact IDs < xmin are considered finished.
 *		All xact IDs >= xmax are considered still running.
 *		For an xact ID xmin <= xid < xmax, consult list to see whether
 *		it is considered running or not.
 * This ensures that the set of transactions seen as "running" by the
 * current xact will not change after it takes the snapshot.
 *
 * All running top-level XIDs are included in the snapshot.  We also try
 * to include running subtransaction XIDs, but since PGPROC has only a
 * limited cache area for subxact XIDs, full information may not be
 * available.  If we find any overflowed subxid arrays, we have to mark
 * the snapshot's subxid data as overflowed, and extra work will need to
 * be done to determine what's running (see XidInSnapshot() in tqual.c).
 *
 * We also update the following backend-global variables:
 *		TransactionXmin: the oldest xmin of any snapshot in use in the
 *			current transaction (this is the same as MyProc->xmin).  This
 *			is just the xmin computed for the first, serializable snapshot.
 *		RecentXmin: the xmin computed for the most recent snapshot.  XIDs
 *			older than this are known not running any more.
 *		RecentGlobalXmin: the global xmin (oldest TransactionXmin across all
 *			running transactions, except those running LAZY VACUUM).  This is
 *			the same computation done by GetOldestXmin(true, true).
 *----------
 */
Snapshot
GetSnapshotData(Snapshot snapshot, bool serializable)
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
		snapshot->xip = (TransactionId *)malloc(arrayP->maxProcs * sizeof(TransactionId));
		if (snapshot->xip == NULL)
		{
			ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
		}

		Assert(snapshot->subxip == NULL);
	}

	if (snapshot->subxip == NULL)
	{
		snapshot->subxip = (TransactionId *)
			malloc(arrayP->maxProcs * PGPROC_MAX_CACHED_SUBXIDS * sizeof(TransactionId));
		if (snapshot->subxip == NULL)
		{
			ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
		}
	}

	/*
	 * GP: Distributed snapshot.
	 */
	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "GetSnapshotData maxCount %d, inProgressEntryArray %p", 
	 	 snapshot->distribSnapshotWithLocalMapping.header.maxCount,
	 	 snapshot->distribSnapshotWithLocalMapping.inProgressEntryArray);
	
	if (snapshot->distribSnapshotWithLocalMapping.inProgressEntryArray == NULL)
	{
		int maxCount;
		
		maxCount = GetDistributedSnapshotMaxCount();
		if (maxCount > 0)
		{
			snapshot->distribSnapshotWithLocalMapping.inProgressEntryArray = 
				(DistributedSnapshotMapEntry *)malloc(maxCount * sizeof(DistributedSnapshotMapEntry));

			if (snapshot->distribSnapshotWithLocalMapping.inProgressEntryArray == NULL)
			{
				ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
			}
			snapshot->distribSnapshotWithLocalMapping.header.maxCount = maxCount;
		}
	}

	/*
	 * MPP Addition.  if we are in EXECUTE mode and not the writer... then we
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

			if (gp_enable_slow_cursor_testmode)
				pg_usleep(2 * 1000 * 1000); /* 1 sec. */

			return snapshot;
		}

		elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),"[Distributed Snapshot #%u] *Start Reader Match* gxid = %u and currcid %d (%s)", 
			 QEDtxContextInfo.distributedSnapshot.header.distribSnapshotId,
			 QEDtxContextInfo.distributedXid,
			 QEDtxContextInfo.curcid,
			 DtxContextToString(DistributedTransactionContext));

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

				elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),
					 "Reader qExec usedComboCids: %d shared %d segmateSync %d",
					 usedComboCids, SharedLocalSnapshotSlot->combocidcnt, SharedLocalSnapshotSlot->segmateSync);
				
				SetSharedTransactionId();

				elog((Debug_print_snapshot_dtm ? LOG : DEBUG5), "Reader qExec setting shared local snapshot to: xmin: %d xmax: %d curcid: %d",
					 snapshot->xmin, snapshot->xmax, snapshot->curcid);
				
				if (Debug_print_full_dtm)
				{
					ShowSubtransactionsForSharedSnapshot();
				}

				GetSubXidsInXidBuffer();

				elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),
					 "GetSnapshotData(): READER currentcommandid %d curcid %d segmatesync %d",
					 GetCurrentCommandId(), snapshot->curcid, SharedLocalSnapshotSlot->segmateSync);

				return snapshot;
			}
			else
			{
				/*
				 * didn't find it.  we'll sleep for a small amount of time and
				 * then try again.
				 *
				 * TODO: is there a semaphore or something better we can do ehre.
				 */
				pg_usleep(sleep_per_check_us);

				CHECK_FOR_INTERRUPTS();

				warning_sleep_time_us += sleep_per_check_us;
				total_sleep_time_us += sleep_per_check_us;

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
									   SharedLocalSnapshotSlot->QDcid,  SharedLocalSnapshotSlot->ready,
									   DtxContextToString(DistributedTransactionContext),
									   SharedLocalSnapshotSlot->slotindex, SharedSnapshotDump())));
				}
				else if (warning_sleep_time_us > 1000 * 1000)
				{
					/*
					 * Every second issue warning.
					 */
					elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),"[Distributed Snapshot #%u] *No Match* gxid %u = %u and currcid %d = %d (%s)", 
						 QEDtxContextInfo.distributedSnapshot.header.distribSnapshotId,
						 QEDtxContextInfo.distributedXid,
						 SharedLocalSnapshotSlot->QDxid,
						 QEDtxContextInfo.curcid,
						 SharedLocalSnapshotSlot->QDcid,
						 DtxContextToString(DistributedTransactionContext));


					elog(LOG,"GetSnapshotData did not find shared local snapshot information. "
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
						 DtxContextToString(DistributedTransactionContext));
					warning_sleep_time_us = 0;
				}

				/* UNDONE: Back-off from checking every millisecond... */
			}
		}
	}

	/* We must not be a reader. */
	Assert(DistributedTransactionContext != DTX_CONTEXT_QE_READER);
	Assert(DistributedTransactionContext != DTX_CONTEXT_QE_ENTRY_DB_SINGLETON);

	/* Serializable snapshot must be computed before any other... */
	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "GetSnapshotData serializable %s, xmin %u", 
	 	 (serializable ? "true" : "false"),
	 	 MyProc->xmin);
	Assert(serializable ?
		   !TransactionIdIsValid(MyProc->xmin) :
		   TransactionIdIsValid(MyProc->xmin));

	globalxmin = xmin = GetTopTransactionId();

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "GetSnapshotData setting globalxmin and xmin to %u", 
	 	 xmin);

	/*
	 * It is sufficient to get shared lock on ProcArrayLock, even if
	 * we are computing a serializable snapshot and therefore will be
	 * setting MyProc->xmin.  This is because any two backends that
	 * have overlapping shared holds on ProcArrayLock will certainly
	 * compute the same xmin (since no xact, in particular not the
	 * oldest, can exit the set of running transactions while we hold
	 * ProcArrayLock --- see further discussion just below). So it
	 * doesn't matter whether another backend concurrently doing
	 * GetSnapshotData or GetOldestXmin sees our xmin as set or not;
	 * he'd compute the same xmin for himself either way.
	 */
	LWLockAcquire(ProcArrayLock, LW_SHARED);

	/*--------------------
	 * Unfortunately, we have to call ReadNewTransactionId() after acquiring
	 * ProcArrayLock above.  It's not good because ReadNewTransactionId() does
	 * LWLockAcquire(XidGenLock), but *necessary*.	We need to be sure that
	 * no transactions exit the set of currently-running transactions
	 * between the time we fetch xmax and the time we finish building our
	 * snapshot.  Otherwise we could have a situation like this:
	 *
	 *		1. Tx Old is running (in Read Committed mode).
	 *		2. Tx S reads new transaction ID into xmax, then
	 *		   is swapped out before acquiring ProcArrayLock.
	 *		3. Tx New gets new transaction ID (>= S' xmax),
	 *		   makes changes and commits.
	 *		4. Tx Old changes some row R changed by Tx New and commits.
	 *		5. Tx S finishes getting its snapshot data.  It sees Tx Old as
	 *		   done, but sees Tx New as still running (since New >= xmax).
	 *
	 * Now S will see R changed by both Tx Old and Tx New, *but* does not
	 * see other changes made by Tx New.  If S is supposed to be in
	 * Serializable mode, this is wrong.
	 *
	 * By locking ProcArrayLock before we read xmax, we ensure that TX Old
	 * cannot exit the set of running transactions seen by Tx S.  Therefore
	 * both Old and New will be seen as still running => no inconsistency.
	 *--------------------
	 */

	xmax = ReadNewTransactionId();

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
	 *      a tuple is visible through a snapshot.  Only committed xids are
	 *      given to XidInSnapshot for evaluation.  XidInSnapshot will first
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
	 * is only evaluated against the distributed snapshot.  And, the scheme
	 * handles local transaction work seeing distributed work properly by
	 * including distributed transactions in the local snapshot via their
	 * local xids.
	 */
	FillInDistributedSnapshot(snapshot);

	/*
	 * Scan the PGPROC array to fill in the local snapshot.
	 */
	for (index = 0; index < arrayP->numProcs; index++)
	{
		volatile PGPROC *proc = arrayP->procs[index];

		/* Fetch xid just once - see GetNewTransactionId */
		TransactionId xid = proc->xid;

		/*
		 * Ignore my own proc (dealt with my xid above), procs not running a
		 * transaction, xacts started since we read the next transaction ID,
		 * and xacts executing LAZY VACUUM. There's no need to store XIDs
		 * above what we got from ReadNewTransactionId, since we'll treat them
		 * as running anyway.  We also assume that such xacts can't compute an
		 * xmin older than ours, so they needn't be considered in computing
		 * globalxmin.
		 */
		if (proc == MyProc ||
			!TransactionIdIsNormal(xid) ||
			TransactionIdFollowsOrEquals(xid, xmax))
		{
			continue;
		}
		
		if (TransactionIdPrecedes(xid, xmin))
			xmin = xid;
		
		snapshot->xip[count] = xid;
		count++;

		/* Update globalxmin to be the smallest valid xmin */
		xid = proc->xmin;
		if (TransactionIdIsNormal(xid))
			if (TransactionIdPrecedes(xid, globalxmin))
				globalxmin = xid;

		/*
		 * Save subtransaction XIDs if possible (if we've already overflowed,
		 * there's no point).  Note that the subxact XIDs must be later than
		 * their parent, so no need to check them against xmin.
		 *
		 * The other backend can add more subxids concurrently, but cannot
		 * remove any.	Hence it's important to fetch nxids just once. Should
		 * be safe to use memcpy, though.  (We needn't worry about missing any
		 * xids added concurrently, because they must postdate xmax.)
		 */
		if (subcount >= 0)
		{
			if (proc->subxids.overflowed)
				subcount = -1;	/* overflowed */
			else
			{
				int			nxids = proc->subxids.nxids;

				if (nxids > 0)
				{
					memcpy(snapshot->subxip + subcount,
						   ((PGPROC *)proc)->subxids.xids,
						   nxids * sizeof(TransactionId));
					subcount += nxids;
				}
			}
		}
	}

	if (serializable)
	{
		/* Not that these values are not set atomically. However,
		 * each of these assignments is itself assumed to be atomic. */
		MyProc->xmin = TransactionXmin = xmin;
	}
	if (IsXactIsoLevelSerializable)
	{
		MyProc->serializableIsoLevel = true;

		elog((Debug_print_snapshot_dtm ? LOG : DEBUG3), "Got serializable snapshot: "
			"database %d, pid %d, xid %d, xmin %d",
			MyProc->databaseId, MyProc->pid, MyProc->xid, MyProc->xmin);
	}

	LWLockRelease(ProcArrayLock);

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

	snapshot->curcid = GetCurrentCommandId();

	/*
	 * MPP Addition.  If we are the chief then we'll save our local snapshot
	 * into the shared snapshot.  Note: we need to use the shared local
	 * snapshot for the "Local Implicit using Distributed Snapshot" case, too.
	 */
	
	if ((DistributedTransactionContext == DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER ||
		 DistributedTransactionContext == DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER ||
		 DistributedTransactionContext == DTX_CONTEXT_QE_AUTO_COMMIT_IMPLICIT) &&
		SharedLocalSnapshotSlot != NULL)
	{
		updateSharedLocalSnapshot(&QEDtxContextInfo, snapshot, "GetSnapshotData");
	}

	elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),
		 "GetSnapshotData(): WRITER currentcommandid %d curcid %d segmatesync %d",
		 GetCurrentCommandId(), snapshot->curcid, QEDtxContextInfo.segmateSync);

	return snapshot;
}

/*
 * MPP: Special code to update the command id in the SharedLocalSnapshot
 * when we are in SERIALIZABLE isolation mode.
 */
void UpdateSerializableCommandId(void)
{
	if ((DistributedTransactionContext == DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER ||
		 DistributedTransactionContext == DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER) &&
		 SharedLocalSnapshotSlot != NULL &&
		 SerializableSnapshot != NULL)
	{
		int combocidSize;

		if (SharedLocalSnapshotSlot->QDxid != QEDtxContextInfo.distributedXid)
		{
			elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),"[Distributed Snapshot #%u] *Can't Update Serializable Command Id* QDxid = %u (gxid = %u, '%s')", 
			 	  QEDtxContextInfo.distributedSnapshot.header.distribSnapshotId,
			 	  SharedLocalSnapshotSlot->QDxid,
			 	  getDistributedTransactionId(),
			 	  DtxContextToString(DistributedTransactionContext));
			return;
		}

		elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),
			 "[Distributed Snapshot #%u] *Update Serializable Command Id* segment currcid = %d, QDcid = %d, SerializableSnapshot currcid = %d, Shared currcid = %d (gxid = %u, '%s')", 
		 	  QEDtxContextInfo.distributedSnapshot.header.distribSnapshotId,
		 	  QEDtxContextInfo.curcid,
		 	  SharedLocalSnapshotSlot->QDcid,
		 	  SerializableSnapshot->curcid,
		 	  SharedLocalSnapshotSlot->snapshot.curcid,
		 	  getDistributedTransactionId(),
		 	  DtxContextToString(DistributedTransactionContext));

		SharedLocalSnapshotSlot->ready = false;

		elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),
			 "serializable writer updating combocid: used combocids %d shared %d", usedComboCids, SharedLocalSnapshotSlot->combocidcnt);

		combocidSize = ((usedComboCids < MaxComboCids) ? usedComboCids : MaxComboCids );

		SharedLocalSnapshotSlot->combocidcnt = combocidSize;	
		memcpy((void *)SharedLocalSnapshotSlot->combocids, comboCids,
			   combocidSize * sizeof(ComboCidKeyData));

		SharedLocalSnapshotSlot->snapshot.curcid = SerializableSnapshot->curcid;
		SharedLocalSnapshotSlot->QDcid = QEDtxContextInfo.curcid;
		SharedLocalSnapshotSlot->segmateSync = QEDtxContextInfo.segmateSync;

		SharedLocalSnapshotSlot->ready = true;
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
		 * PGPROC struct, though, because freed PGPPROC entries just go to the
		 * free list and are recycled. Its contents are nonsense in that case,
		 * but that's acceptable for this function.
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
 * CheckOtherDBBackends -- check for other backends running in the given DB
 *
 * If there are other backends in the DB, we will wait a maximum of 5 seconds
 * for them to exit.  Autovacuum backends are encouraged to exit early by
 * sending them SIGTERM, but normal user backends are just waited for.
 *
 * The current backend is always ignored; it is caller's responsibility to
 * check whether the current backend uses the given DB, if it's important.
 *
 * Returns TRUE if there are (still) other backends in the DB, FALSE if not.
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
CheckOtherDBBackends(Oid databaseId)
{
	ProcArrayStruct *arrayP = procArray;
	int			tries;

	/* 50 tries with 100ms sleep between tries makes 5 sec total wait */
	for (tries = 0; tries < 50; tries++)
	{
		bool		found = false;
		int			index;

		CHECK_FOR_INTERRUPTS();

		LWLockAcquire(ProcArrayLock, LW_SHARED);

		for (index = 0; index < arrayP->numProcs; index++)
		{
			volatile PGPROC *proc = arrayP->procs[index];

			if (proc->databaseId != databaseId)
				continue;
			if (proc == MyProc)
				continue;

			found = true;

			if (proc->isAutovacuum)
			{
				/* an autovacuum --- send it SIGTERM before sleeping */
				int			autopid = proc->pid;

				/*
				 * It's a bit awkward to release ProcArrayLock within the
				 * loop, but we'd probably better do so before issuing kill().
				 * We have no idea what might block kill() inside the
				 * kernel...
				 */
				LWLockRelease(ProcArrayLock);

				(void) kill(autopid, SIGTERM);	/* ignore any error */

				break;
			}
			else
			{
				LWLockRelease(ProcArrayLock);
				break;
			}
		}

		/* if found is set, we released the lock within the loop body */
		if (!found)
		{
			LWLockRelease(ProcArrayLock);
			return false;		/* no conflicting backends, so done */
		}

		/* else sleep and try again */
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
 */
void
XidCacheRemoveRunningXids(TransactionId xid, int nxids, TransactionId *xids)
{
	int			i,
				j;

	Assert(TransactionIdIsValid(xid));

	/*
	 * We must hold ProcArrayLock exclusively in order to remove transactions
	 * from the PGPROC array.  (See notes in GetSnapshotData.)	It's possible
	 * this could be relaxed since we know this routine is only used to abort
	 * subtransactions, but pending closer analysis we'd best be conservative.
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
			"XidCache: xmin: %ld, mainxid: %ld, childxid: %ld, slow: %ld\n",
			xc_by_recent_xmin,
			xc_by_main_xid,
			xc_by_child_xid,
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
