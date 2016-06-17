/*-------------------------------------------------------------------------
 *
 * tqual.c
 *	  POSTGRES "time qualification" code, ie, tuple visibility rules.
 *
 * NOTE: all the HeapTupleSatisfies routines will update the tuple's
 * "hint" status bits if we see that the inserting or deleting transaction
 * has now committed or aborted (and it is safe to set the hint bits).
 * If the hint bits are changed, SetBufferCommitInfoNeedsSave is called on
 * the passed-in buffer.  The caller must hold not only a pin, but at least
 * shared buffer content lock on the buffer containing the tuple.
 *
 * NOTE: must check TransactionIdIsInProgress (which looks in PGPROC array)
 * before TransactionIdDidCommit/TransactionIdDidAbort (which look in
 * pg_clog).  Otherwise we have a race condition: we might decide that a
 * just-committed transaction crashed, because none of the tests succeed.
 * xact.c is careful to record commit/abort in pg_clog before it unsets
 * MyProc->xid in PGPROC array.  That fixes that problem, but it also
 * means there is a window where TransactionIdIsInProgress and
 * TransactionIdDidCommit will both return true.  If we check only
 * TransactionIdDidCommit, we could consider a tuple committed when a
 * later GetSnapshotData call will still think the originating transaction
 * is in progress, which leads to application-level inconsistency.	The
 * upshot is that we gotta check TransactionIdIsInProgress first in all
 * code paths, except for a few cases where we are looking at
 * subtransactions of our own main transaction and so there can't be any
 * race condition.
 *
 * Summary of visibility functions:
 *
 *   HeapTupleSatisfiesMVCC()
 *        visible to supplied snapshot, excludes current command
 *   HeapTupleSatisfiesNow()
 *        visible to instant snapshot, excludes current command
 *   HeapTupleSatisfiesUpdate()
 *        like HeapTupleSatisfiesNow(), but with user-supplied command
 *        counter and more complex result
 *   HeapTupleSatisfiesSelf()
 *        visible to instant snapshot and current command
 *   HeapTupleSatisfiesDirty()
 *        like HeapTupleSatisfiesSelf(), but includes open transactions
 *   HeapTupleSatisfiesVacuum()
 *        visible to any running transaction, used by VACUUM
 *   HeapTupleSatisfiesToast()
 *        visible unless part of interrupted vacuum, used for TOAST
 *   HeapTupleSatisfiesAny()
 *        all tuples are visible
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/utils/time/tqual.c,v 1.109.2.1 2008/09/11 14:01:35 alvherre Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/multixact.h"
#include "access/subtrans.h"
#include "access/transam.h"
#include "access/xact.h"
#include "storage/bufmgr.h"
#include "storage/procarray.h"
#include "utils/tqual.h"

#include "lib/stringinfo.h"

#include "funcapi.h"

#include "utils/builtins.h"

#include "cdb/cdbtm.h"
#include "cdb/cdbvars.h"

#include "access/clog.h"

#include "access/distributedlog.h"

/* Static variables representing various special snapshot semantics */
SnapshotData SnapshotNowData = {HeapTupleSatisfiesNow};
SnapshotData SnapshotSelfData = {HeapTupleSatisfiesSelf};
SnapshotData SnapshotAnyData = {HeapTupleSatisfiesAny};
SnapshotData SnapshotToastData = {HeapTupleSatisfiesToast};

/*
 * These SnapshotData structs are static to simplify memory allocation
 * (see the hack in GetSnapshotData to avoid repeated malloc/free).
 */
static SnapshotData SerializableSnapshotData = {HeapTupleSatisfiesMVCC};
static SnapshotData LatestSnapshotData = {HeapTupleSatisfiesMVCC};

/* Externally visible pointers to valid snapshots: */
Snapshot	SerializableSnapshot = NULL;
Snapshot	LatestSnapshot = NULL;

/*
 * This pointer is not maintained by this module, but it's convenient
 * to declare it here anyway.  Callers typically assign a copy of
 * GetTransactionSnapshot's result to ActiveSnapshot.
 */
Snapshot	ActiveSnapshot = NULL;

/*
 * These are updated by GetSnapshotData.  We initialize them this way
 * for the convenience of TransactionIdIsInProgress: even in bootstrap
 * mode, we don't want it to say that BootstrapTransactionId is in progress.
 *
 * RecentGlobalXmin is initialized to InvalidTransactionId, to ensure that no
 * one tries to use a stale value.  Readers should ensure that it has been set
 * to something else before using it.
 */
TransactionId TransactionXmin = FirstNormalTransactionId;
TransactionId RecentXmin = FirstNormalTransactionId;
TransactionId RecentGlobalXmin = InvalidTransactionId;


/* local functions */
static bool XidInMVCCSnapshot(TransactionId xid, Snapshot snapshot, bool isXmax,
			  bool distributedSnapshotIgnore, bool *setDistributedSnapshotIgnore);
static bool XidInMVCCSnapshot_Local(TransactionId xid, Snapshot snapshot, bool isXmax);

/*
 * Set the buffer dirty after setting t_infomask
 */
static inline void
markDirty(Buffer buffer, Relation relation, HeapTupleHeader tuple, bool isXmin)
{
	TransactionId xid;

	if (!gp_disable_tuple_hints)
	{
		SetBufferCommitInfoNeedsSave(buffer);
		return;
	}

	/*
	 * The GUC gp_disable_tuple_hints is on.  Do further evaluation whether we want to write out the
	 * buffer or not.
	 */
	if (relation == NULL)
	{
		SetBufferCommitInfoNeedsSave(buffer);
		return;
	}

	if (relation->rd_issyscat)
	{
		/* Assume we want to always mark the buffer dirty */
		SetBufferCommitInfoNeedsSave(buffer);
		return;
	}

	/*
	 * Get the xid whose hint bits were just set.
	 */
	if (isXmin)
		xid = HeapTupleHeaderGetXmin(tuple);
	else
		xid = HeapTupleHeaderGetXmax(tuple);

	if (xid == InvalidTransactionId)
	{
		SetBufferCommitInfoNeedsSave(buffer);
		return;
	}

	/*
	 * Check age of the affected xid.  If it is too old, mark the buffer to be written.
	 */
	if (CLOGTransactionIsOld(xid))
	{
		SetBufferCommitInfoNeedsSave(buffer);
		return;
	}
}

/*
 * SetHintBits()
 *
 * Set commit/abort hint bits on a tuple, if appropriate at this time.
 *
 * It is only safe to set a transaction-committed hint bit if we know the
 * transaction's commit record has been flushed to disk.  We cannot change
 * the LSN of the page here because we may hold only a share lock on the
 * buffer, so we can't use the LSN to interlock this; we have to just refrain
 * from setting the hint bit until some future re-examination of the tuple.
 *
 * We can always set hint bits when marking a transaction aborted.	(Some
 * code in heapam.c relies on that!)
 *
 * Also, if we are cleaning up HEAP_MOVED_IN or HEAP_MOVED_OFF entries, then
 * we can always set the hint bits, since VACUUM FULL always uses synchronous
 * commits and doesn't move tuples that weren't previously hinted.	(This is
 * not known by this subroutine, but is applied by its callers.)
 *
 * Normal commits may be asynchronous, so for those we need to get the LSN
 * of the transaction and then check whether this is flushed.
 *
 * The caller should pass xid as the XID of the transaction to check, or
 * InvalidTransactionId if no check is needed.
 */
static inline void
SetHintBits(HeapTupleHeader tuple, Buffer buffer, Relation rel,
			uint16 infomask, TransactionId xid)
{
	bool		isXmin;

	if (TransactionIdIsValid(xid))
	{
		/* NB: xid must be known committed here! */
		XLogRecPtr	commitLSN = TransactionIdGetCommitLSN(xid);

		if (XLogNeedsFlush(commitLSN))
			return;				/* not flushed yet, so don't set hint */
	}

	tuple->t_infomask |= infomask;

	switch(infomask)
	{
		case HEAP_XMIN_INVALID:
		case HEAP_XMIN_COMMITTED:
			isXmin = true;
			break;
		case HEAP_XMAX_INVALID:
		case HEAP_XMAX_COMMITTED:
			isXmin = false;
			break;
		default:
			elog(ERROR, "unexpected infomask while setting hint bits: %d", infomask);
			isXmin = false; /* keep compiler quiet */
	}

	markDirty(buffer, rel, tuple, isXmin);
}

/*
 * HeapTupleSetHintBits --- exported version of SetHintBits()
 *
 * This must be separate because of C99's brain-dead notions about how to
 * implement inline functions.
 */
void
HeapTupleSetHintBits(HeapTupleHeader tuple, Buffer buffer, Relation rel,
					 uint16 infomask, TransactionId xid)
{
	SetHintBits(tuple, buffer, rel, infomask, xid);
}

/*
 * HeapTupleSatisfiesSelf
 *		True iff heap tuple is valid "for itself".
 *
 *	Here, we consider the effects of:
 *		all committed transactions (as of the current instant)
 *		previous commands of this transaction
 *		changes made by the current command
 *
 * Note:
 *		Assumes heap tuple is valid.
 *
 * The satisfaction of "itself" requires the following:
 *
 * ((Xmin == my-transaction &&				the row was updated by the current transaction, and
 *		(Xmax is null						it was not deleted
 *		 [|| Xmax != my-transaction)])			[or it was deleted by another transaction]
 * ||
 *
 * (Xmin is committed &&					the row was modified by a committed transaction, and
 *		(Xmax is null ||					the row has not been deleted, or
 *			(Xmax != my-transaction &&			the row was deleted by another transaction
 *			 Xmax is not committed)))			that has not been committed
 */
bool
HeapTupleSatisfiesSelf(Relation relation, HeapTupleHeader tuple, Snapshot snapshot, Buffer buffer)
{
	if (!(tuple->t_infomask & HEAP_XMIN_COMMITTED))
	{
		if (tuple->t_infomask & HEAP_XMIN_INVALID)
			return false;

		if (tuple->t_infomask & HEAP_MOVED_OFF)
		{
			TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

			if (TransactionIdIsCurrentTransactionId(xvac))
				return false;
			if (!TransactionIdIsInProgress(xvac))
			{
				if (TransactionIdDidCommit(xvac))
				{
					SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
								InvalidTransactionId);
					return false;
				}
				SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
							InvalidTransactionId);
			}
		}
		else if (tuple->t_infomask & HEAP_MOVED_IN)
		{
			TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

			if (!TransactionIdIsCurrentTransactionId(xvac))
			{
				if (TransactionIdIsInProgress(xvac))
					return false;
				if (TransactionIdDidCommit(xvac))
					SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
								InvalidTransactionId);
				else
				{
					SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
								InvalidTransactionId);
					return false;
				}
			}
		}
		else if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmin(tuple)))
		{
			if (tuple->t_infomask & HEAP_XMAX_INVALID)	/* xid invalid */
				return true;

			if (tuple->t_infomask & HEAP_IS_LOCKED)		/* not deleter */
				return true;

			Assert(!(tuple->t_infomask & HEAP_XMAX_IS_MULTI));

			if (!TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmax(tuple)))
			{
				/* deleting subtransaction must have aborted */
				SetHintBits(tuple, buffer, relation, HEAP_XMAX_INVALID,
							InvalidTransactionId);
				return true;
			}

			return false;
		}
		else if (TransactionIdIsInProgress(HeapTupleHeaderGetXmin(tuple)))
			return false;
		else if (TransactionIdDidCommit(HeapTupleHeaderGetXmin(tuple)))
			SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
						HeapTupleHeaderGetXmin(tuple));
		else
		{
			/* it must have aborted or crashed */
			SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
						InvalidTransactionId);
			return false;
		}
	}

	/* by here, the inserting transaction has committed */

	if (tuple->t_infomask & HEAP_XMAX_INVALID)	/* xid invalid or aborted */
		return true;

	if (tuple->t_infomask & HEAP_XMAX_COMMITTED)
	{
		if (tuple->t_infomask & HEAP_IS_LOCKED)
			return true;
		return false;			/* updated by other */
	}

	if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
	{
		/* MultiXacts are currently only allowed to lock tuples */
		Assert(tuple->t_infomask & HEAP_IS_LOCKED);
		return true;
	}

	if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmax(tuple)))
	{
		if (tuple->t_infomask & HEAP_IS_LOCKED)
			return true;
		return false;
	}

	if (TransactionIdIsInProgress(HeapTupleHeaderGetXmax(tuple)))
		return true;

	if (!TransactionIdDidCommit(HeapTupleHeaderGetXmax(tuple)))
	{
		/* it must have aborted or crashed */
		SetHintBits(tuple, buffer, relation, HEAP_XMAX_INVALID,
					InvalidTransactionId);
		return true;
	}

	/* xmax transaction committed */

	if (tuple->t_infomask & HEAP_IS_LOCKED)
	{
		SetHintBits(tuple, buffer, relation, HEAP_XMAX_INVALID,
					InvalidTransactionId);
		return true;
	}

	SetHintBits(tuple, buffer, relation, HEAP_XMAX_COMMITTED,
				HeapTupleHeaderGetXmax(tuple));
	return false;
}

/*
 * HeapTupleSatisfiesNow
 *		True iff heap tuple is valid "now".
 *
 *	Here, we consider the effects of:
 *		all committed transactions (as of the current instant)
 *		previous commands of this transaction
 *
 * Note we do _not_ include changes made by the current command.  This
 * solves the "Halloween problem" wherein an UPDATE might try to re-update
 * its own output tuples.
 *
 * Note:
 *		Assumes heap tuple is valid.
 *
 * The satisfaction of "now" requires the following:
 *
 * ((Xmin == my-transaction &&				inserted by the current transaction
 *	 Cmin < my-command &&					before this command, and
 *	 (Xmax is null ||						the row has not been deleted, or
 *	  (Xmax == my-transaction &&			it was deleted by the current transaction
 *	   Cmax >= my-command)))				but not before this command,
 * ||										or
 *	(Xmin is committed &&					the row was inserted by a committed transaction, and
 *		(Xmax is null ||					the row has not been deleted, or
 *		 (Xmax == my-transaction &&			the row is being deleted by this transaction
 *		  Cmax >= my-command) ||			but it's not deleted "yet", or
 *		 (Xmax != my-transaction &&			the row was deleted by another transaction
 *		  Xmax is not committed))))			that has not been committed
 *
 *		mao says 17 march 1993:  the tests in this routine are correct;
 *		if you think they're not, you're wrong, and you should think
 *		about it again.  i know, it happened to me.  we don't need to
 *		check commit time against the start time of this transaction
 *		because 2ph locking protects us from doing the wrong thing.
 *		if you mess around here, you'll break serializability.  the only
 *		problem with this code is that it does the wrong thing for system
 *		catalog updates, because the catalogs aren't subject to 2ph, so
 *		the serializability guarantees we provide don't extend to xacts
 *		that do catalog accesses.  this is unfortunate, but not critical.
 */
bool
HeapTupleSatisfiesNow(Relation relation, HeapTupleHeader tuple, Snapshot snapshot, Buffer buffer)
{
	if (!(tuple->t_infomask & HEAP_XMIN_COMMITTED))
	{
		if (tuple->t_infomask & HEAP_XMIN_INVALID)
			return false;

		if (tuple->t_infomask & HEAP_MOVED_OFF)
		{
			TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

			if (TransactionIdIsCurrentTransactionId(xvac))
				return false;
			if (!TransactionIdIsInProgress(xvac))
			{
				if (TransactionIdDidCommit(xvac))
				{
					SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
								InvalidTransactionId);
					return false;
				}
				SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
							InvalidTransactionId);
			}
		}
		else if (tuple->t_infomask & HEAP_MOVED_IN)
		{
			TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

			if (!TransactionIdIsCurrentTransactionId(xvac))
			{
				if (TransactionIdIsInProgress(xvac))
					return false;
				if (TransactionIdDidCommit(xvac))
					SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
								InvalidTransactionId);
				else
				{
					SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
								InvalidTransactionId);
					return false;
				}
			}
		}
		else if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmin(tuple)))
		{
			if (HeapTupleHeaderGetCmin(tuple) >= GetCurrentCommandId(false))
				return false;	/* inserted after scan started */

			if (tuple->t_infomask & HEAP_XMAX_INVALID)	/* xid invalid */
				return true;

			if (tuple->t_infomask & HEAP_IS_LOCKED)		/* not deleter */
				return true;

			Assert(!(tuple->t_infomask & HEAP_XMAX_IS_MULTI));

			if (!TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmax(tuple)))
			{
				/* deleting subtransaction must have aborted */
				SetHintBits(tuple, buffer, relation, HEAP_XMAX_INVALID,
							InvalidTransactionId);
				return true;
			}

			if (HeapTupleHeaderGetCmax(tuple) >= GetCurrentCommandId(false))
				return true;	/* deleted after scan started */
			else
				return false;	/* deleted before scan started */
		}
		else if (TransactionIdIsInProgress(HeapTupleHeaderGetXmin(tuple)))
			return false;
		else if (TransactionIdDidCommit(HeapTupleHeaderGetXmin(tuple)))
			SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
						HeapTupleHeaderGetXmin(tuple));
		else
		{
			/* it must have aborted or crashed */
			SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
						InvalidTransactionId);
			return false;
		}
	}

	/* by here, the inserting transaction has committed */

	if (tuple->t_infomask & HEAP_XMAX_INVALID)	/* xid invalid or aborted */
		return true;

	if (tuple->t_infomask & HEAP_XMAX_COMMITTED)
	{
		if (tuple->t_infomask & HEAP_IS_LOCKED)
			return true;
		return false;
	}

	if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
	{
		/* MultiXacts are currently only allowed to lock tuples */
		Assert(tuple->t_infomask & HEAP_IS_LOCKED);
		return true;
	}

	if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmax(tuple)))
	{
		if (tuple->t_infomask & HEAP_IS_LOCKED)
			return true;
		if (HeapTupleHeaderGetCmax(tuple) >= GetCurrentCommandId(false))
			return true;		/* deleted after scan started */
		else
			return false;		/* deleted before scan started */
	}

	if (TransactionIdIsInProgress(HeapTupleHeaderGetXmax(tuple)))
		return true;

	if (!TransactionIdDidCommit(HeapTupleHeaderGetXmax(tuple)))
	{
		/* it must have aborted or crashed */
		SetHintBits(tuple, buffer, relation, HEAP_XMAX_INVALID,
					InvalidTransactionId);
		return true;
	}

	/* xmax transaction committed */

	if (tuple->t_infomask & HEAP_IS_LOCKED)
	{
		SetHintBits(tuple, buffer, relation, HEAP_XMAX_INVALID,
					InvalidTransactionId);
		return true;
	}

	SetHintBits(tuple, buffer, relation, HEAP_XMAX_COMMITTED,
				HeapTupleHeaderGetXmax(tuple));
	return false;
}

/*
 * HeapTupleSatisfiesAny
 *		Dummy "satisfies" routine: any tuple satisfies SnapshotAny.
 */
bool
HeapTupleSatisfiesAny(Relation relation, HeapTupleHeader tuple, Snapshot snapshot, Buffer buffer)
{
	return true;
}

/*
 * HeapTupleSatisfiesToast
 *		True iff heap tuple is valid as a TOAST row.
 *
 * This is a simplified version that only checks for VACUUM moving conditions.
 * It's appropriate for TOAST usage because TOAST really doesn't want to do
 * its own time qual checks; if you can see the main table row that contains
 * a TOAST reference, you should be able to see the TOASTed value.	However,
 * vacuuming a TOAST table is independent of the main table, and in case such
 * a vacuum fails partway through, we'd better do this much checking.
 *
 * Among other things, this means you can't do UPDATEs of rows in a TOAST
 * table.
 */
bool
HeapTupleSatisfiesToast(Relation relation, HeapTupleHeader tuple, Snapshot snapshot,
						Buffer buffer)
{
	if (!(tuple->t_infomask & HEAP_XMIN_COMMITTED))
	{
		if (tuple->t_infomask & HEAP_XMIN_INVALID)
			return false;

		if (tuple->t_infomask & HEAP_MOVED_OFF)
		{
			TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

			if (TransactionIdIsCurrentTransactionId(xvac))
				return false;
			if (!TransactionIdIsInProgress(xvac))
			{
				if (TransactionIdDidCommit(xvac))
				{
					SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
								InvalidTransactionId);
					return false;
				}
				SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
							InvalidTransactionId);
			}
		}
		else if (tuple->t_infomask & HEAP_MOVED_IN)
		{
			TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

			if (!TransactionIdIsCurrentTransactionId(xvac))
			{
				if (TransactionIdIsInProgress(xvac))
					return false;
				if (TransactionIdDidCommit(xvac))
					SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
								InvalidTransactionId);
				else
				{
					SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
								InvalidTransactionId);
					return false;
				}
			}
		}
	}

	/* otherwise assume the tuple is valid for TOAST. */
	return true;
}

/*
 * HeapTupleSatisfiesUpdate
 *
 *	Same logic as HeapTupleSatisfiesNow, but returns a more detailed result
 *	code, since UPDATE needs to know more than "is it visible?".  Also,
 *	tuples of my own xact are tested against the passed CommandId not
 *	CurrentCommandId.
 *
 *	The possible return codes are:
 *
 *	HeapTupleInvisible: the tuple didn't exist at all when the scan started,
 *	e.g. it was created by a later CommandId.
 *
 *	HeapTupleMayBeUpdated: The tuple is valid and visible, so it may be
 *	updated.
 *
 *	HeapTupleSelfUpdated: The tuple was updated by the current transaction,
 *	after the current scan started.
 *
 *	HeapTupleUpdated: The tuple was updated by a committed transaction.
 *
 *	HeapTupleBeingUpdated: The tuple is being updated by an in-progress
 *	transaction other than the current transaction.  (Note: this includes
 *	the case where the tuple is share-locked by a MultiXact, even if the
 *	MultiXact includes the current transaction.  Callers that want to
 *	distinguish that case must test for it themselves.)
 */
HTSU_Result
HeapTupleSatisfiesUpdate(Relation relation, HeapTupleHeader tuple, CommandId curcid,
						 Buffer buffer)
{
	if (!(tuple->t_infomask & HEAP_XMIN_COMMITTED))
	{
		if (tuple->t_infomask & HEAP_XMIN_INVALID)
			return HeapTupleInvisible;

		if (tuple->t_infomask & HEAP_MOVED_OFF)
		{
			TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

			if (TransactionIdIsCurrentTransactionId(xvac))
				return HeapTupleInvisible;
			if (!TransactionIdIsInProgress(xvac))
			{
				if (TransactionIdDidCommit(xvac))
				{
					SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
								InvalidTransactionId);
					return HeapTupleInvisible;
				}
				SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
							InvalidTransactionId);
			}
		}
		else if (tuple->t_infomask & HEAP_MOVED_IN)
		{
			TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

			if (!TransactionIdIsCurrentTransactionId(xvac))
			{
				if (TransactionIdIsInProgress(xvac))
					return HeapTupleInvisible;
				if (TransactionIdDidCommit(xvac))
					SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
								InvalidTransactionId);
				else
				{
					SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
								InvalidTransactionId);
					return HeapTupleInvisible;
				}
			}
		}
		else if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmin(tuple)))
		{
			if (HeapTupleHeaderGetCmin(tuple) >= curcid)
				return HeapTupleInvisible;		/* inserted after scan started */

			if (tuple->t_infomask & HEAP_XMAX_INVALID)	/* xid invalid */
				return HeapTupleMayBeUpdated;

			if (tuple->t_infomask & HEAP_IS_LOCKED)		/* not deleter */
				return HeapTupleMayBeUpdated;

			Assert(!(tuple->t_infomask & HEAP_XMAX_IS_MULTI));

			if (!TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmax(tuple)))
			{
				/* deleting subtransaction must have aborted */
				SetHintBits(tuple, buffer, relation, HEAP_XMAX_INVALID,
							InvalidTransactionId);
				return HeapTupleMayBeUpdated;
			}

			if (HeapTupleHeaderGetCmax(tuple) >= curcid)
				return HeapTupleSelfUpdated;	/* updated after scan started */
			else
				return HeapTupleInvisible;		/* updated before scan started */
		}
		else if (TransactionIdIsInProgress(HeapTupleHeaderGetXmin(tuple)))
			return HeapTupleInvisible;
		else if (TransactionIdDidCommit(HeapTupleHeaderGetXmin(tuple)))
			SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
						HeapTupleHeaderGetXmin(tuple));
		else
		{
			/* it must have aborted or crashed */
			SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
						InvalidTransactionId);
			return HeapTupleInvisible;
		}
	}

	/* by here, the inserting transaction has committed */

	if (tuple->t_infomask & HEAP_XMAX_INVALID)	/* xid invalid or aborted */
		return HeapTupleMayBeUpdated;

	if (tuple->t_infomask & HEAP_XMAX_COMMITTED)
	{
		if (tuple->t_infomask & HEAP_IS_LOCKED)
			return HeapTupleMayBeUpdated;
		return HeapTupleUpdated;	/* updated by other */
	}

	if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
	{
		/* MultiXacts are currently only allowed to lock tuples */
		Assert(tuple->t_infomask & HEAP_IS_LOCKED);

		if (MultiXactIdIsRunning(HeapTupleHeaderGetXmax(tuple)))
			return HeapTupleBeingUpdated;
		SetHintBits(tuple, buffer, relation, HEAP_XMAX_INVALID,
					InvalidTransactionId);
		return HeapTupleMayBeUpdated;
	}

	if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmax(tuple)))
	{
		if (tuple->t_infomask & HEAP_IS_LOCKED)
			return HeapTupleMayBeUpdated;
		if (HeapTupleHeaderGetCmax(tuple) >= curcid)
			return HeapTupleSelfUpdated;		/* updated after scan started */
		else
			return HeapTupleInvisible;	/* updated before scan started */
	}

	if (TransactionIdIsInProgress(HeapTupleHeaderGetXmax(tuple)))
		return HeapTupleBeingUpdated;

	if (!TransactionIdDidCommit(HeapTupleHeaderGetXmax(tuple)))
	{
		/* it must have aborted or crashed */
		SetHintBits(tuple, buffer, relation, HEAP_XMAX_INVALID,
					InvalidTransactionId);
		return HeapTupleMayBeUpdated;
	}

	/* xmax transaction committed */

	if (tuple->t_infomask & HEAP_IS_LOCKED)
	{
		SetHintBits(tuple, buffer, relation, HEAP_XMAX_INVALID,
					InvalidTransactionId);
		return HeapTupleMayBeUpdated;
	}

	SetHintBits(tuple, buffer, relation, HEAP_XMAX_COMMITTED,
				HeapTupleHeaderGetXmax(tuple));
	return HeapTupleUpdated;	/* updated by other */
}

/*
 * HeapTupleSatisfiesDirty
 *		True iff heap tuple is valid including effects of open transactions.
 *
 *	Here, we consider the effects of:
 *		all committed and in-progress transactions (as of the current instant)
 *		previous commands of this transaction
 *		changes made by the current command
 *
 * This is essentially like HeapTupleSatisfiesSelf as far as effects of
 * the current transaction and committed/aborted xacts are concerned.
 * However, we also include the effects of other xacts still in progress.
 *
 * A special hack is that the passed-in snapshot struct is used as an
 * output argument to return the xids of concurrent xacts that affected the
 * tuple.  snapshot->xmin is set to the tuple's xmin if that is another
 * transaction that's still in progress; or to InvalidTransactionId if the
 * tuple's xmin is committed good, committed dead, or my own xact.  Similarly
 * for snapshot->xmax and the tuple's xmax.
 */
bool
HeapTupleSatisfiesDirty(Relation relation, HeapTupleHeader tuple, Snapshot snapshot,
						Buffer buffer)
{
	snapshot->xmin = snapshot->xmax = InvalidTransactionId;

	if (!(tuple->t_infomask & HEAP_XMIN_COMMITTED))
	{
		if (tuple->t_infomask & HEAP_XMIN_INVALID)
			return false;

		if (tuple->t_infomask & HEAP_MOVED_OFF)
		{
			TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

			if (TransactionIdIsCurrentTransactionId(xvac))
				return false;
			if (!TransactionIdIsInProgress(xvac))
			{
				if (TransactionIdDidCommit(xvac))
				{
					SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
								InvalidTransactionId);
					return false;
				}
				SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
							InvalidTransactionId);
			}
		}
		else if (tuple->t_infomask & HEAP_MOVED_IN)
		{
			TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

			if (!TransactionIdIsCurrentTransactionId(xvac))
			{
				if (TransactionIdIsInProgress(xvac))
					return false;
				if (TransactionIdDidCommit(xvac))
					SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
								InvalidTransactionId);
				else
				{
					SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
								InvalidTransactionId);
					return false;
				}
			}
		}
		else if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmin(tuple)))
		{
			if (tuple->t_infomask & HEAP_XMAX_INVALID)	/* xid invalid */
				return true;

			if (tuple->t_infomask & HEAP_IS_LOCKED)		/* not deleter */
				return true;

			Assert(!(tuple->t_infomask & HEAP_XMAX_IS_MULTI));

			if (!TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmax(tuple)))
			{
				/* deleting subtransaction must have aborted */
				SetHintBits(tuple, buffer, relation, HEAP_XMAX_INVALID,
							InvalidTransactionId);
				return true;
			}

			return false;
		}
		else if (TransactionIdIsInProgress(HeapTupleHeaderGetXmin(tuple)))
		{
			snapshot->xmin = HeapTupleHeaderGetXmin(tuple);
			/* XXX shouldn't we fall through to look at xmax? */
			return true;		/* in insertion by other */
		}
		else if (TransactionIdDidCommit(HeapTupleHeaderGetXmin(tuple)))
			SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
						HeapTupleHeaderGetXmin(tuple));
		else
		{
			/* it must have aborted or crashed */
			SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
						InvalidTransactionId);
			return false;
		}
	}

	/* by here, the inserting transaction has committed */

	if (tuple->t_infomask & HEAP_XMAX_INVALID)	/* xid invalid or aborted */
		return true;

	if (tuple->t_infomask & HEAP_XMAX_COMMITTED)
	{
		if (tuple->t_infomask & HEAP_IS_LOCKED)
			return true;
		return false;			/* updated by other */
	}

	if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
	{
		/* MultiXacts are currently only allowed to lock tuples */
		Assert(tuple->t_infomask & HEAP_IS_LOCKED);
		return true;
	}

	if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmax(tuple)))
	{
		if (tuple->t_infomask & HEAP_IS_LOCKED)
			return true;
		return false;
	}

	if (TransactionIdIsInProgress(HeapTupleHeaderGetXmax(tuple)))
	{
		snapshot->xmax = HeapTupleHeaderGetXmax(tuple);
		return true;
	}

	if (!TransactionIdDidCommit(HeapTupleHeaderGetXmax(tuple)))
	{
		/* it must have aborted or crashed */
		SetHintBits(tuple, buffer, relation, HEAP_XMAX_INVALID,
					InvalidTransactionId);
		return true;
	}

	/* xmax transaction committed */

	if (tuple->t_infomask & HEAP_IS_LOCKED)
	{
		SetHintBits(tuple, buffer, relation, HEAP_XMAX_INVALID,
					InvalidTransactionId);
		return true;
	}

	SetHintBits(tuple, buffer, relation, HEAP_XMAX_COMMITTED,
				HeapTupleHeaderGetXmax(tuple));
	return false;				/* updated by other */
}

/*
 * HeapTupleSatisfiesMVCC
 *		True iff heap tuple is valid for the given MVCC snapshot.
 *
 *	Here, we consider the effects of:
 *		all transactions committed as of the time of the given snapshot
 *		previous commands of this transaction
 *
 *	Does _not_ include:
 *		transactions shown as in-progress by the snapshot
 *		transactions started after the snapshot was taken
 *		changes made by the current command
 *
 * This is the same as HeapTupleSatisfiesNow, except that transactions that
 * were in progress or as yet unstarted when the snapshot was taken will
 * be treated as uncommitted, even if they have committed by now.
 *
 * (Notice, however, that the tuple status hint bits will be updated on the
 * basis of the true state of the transaction, even if we then pretend we
 * can't see it.)
 */
bool
HeapTupleSatisfiesMVCC(Relation relation, HeapTupleHeader tuple, Snapshot snapshot,
					   Buffer buffer)
{
	bool inSnapshot = false;
	bool setDistributedSnapshotIgnore = false;

	if (!(tuple->t_infomask & HEAP_XMIN_COMMITTED))
	{
		if (tuple->t_infomask & HEAP_XMIN_INVALID)
			return false;

		if (tuple->t_infomask & HEAP_MOVED_OFF)
		{
			TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

			if (TransactionIdIsCurrentTransactionId(xvac))
				return false;
			if (!TransactionIdIsInProgress(xvac))
			{
				if (TransactionIdDidCommit(xvac))
				{
					SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
								InvalidTransactionId);
					return false;
				}
				SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
							InvalidTransactionId);
			}
		}
		else if (tuple->t_infomask & HEAP_MOVED_IN)
		{
			TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

			if (!TransactionIdIsCurrentTransactionId(xvac))
			{
				if (TransactionIdIsInProgress(xvac))
					return false;
				if (TransactionIdDidCommit(xvac))
					SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
								InvalidTransactionId);
				else
				{
					SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
								InvalidTransactionId);
					return false;
				}
			}
		}
		else if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmin(tuple)))
		{
			if (HeapTupleHeaderGetCmin(tuple) >= snapshot->curcid)
				return false;	/* inserted after scan started */

			if (tuple->t_infomask & HEAP_XMAX_INVALID)	/* xid invalid */
				return true;

			if (tuple->t_infomask & HEAP_IS_LOCKED)		/* not deleter */
				return true;

			Assert(!(tuple->t_infomask & HEAP_XMAX_IS_MULTI));

			if (!TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmax(tuple)))
			{
				/* deleting subtransaction must have aborted */
				SetHintBits(tuple, buffer, relation, HEAP_XMAX_INVALID,
							InvalidTransactionId);
				return true;
			}

			/*
			 * MPP-8317: cursors can't always *tell* that this is the current transaction.
			 */
			Assert(QEDtxContextInfo.cursorContext || TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmax(tuple)));

			if (HeapTupleHeaderGetCmax(tuple) >= snapshot->curcid)
				return true;	/* deleted after scan started */
			else
				return false;	/* deleted before scan started */
		}
		else if (TransactionIdIsInProgress(HeapTupleHeaderGetXmin(tuple)))
			return false;
		else if (TransactionIdDidCommit(HeapTupleHeaderGetXmin(tuple)))
			SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
						HeapTupleHeaderGetXmin(tuple));
		else
		{
			/* it must have aborted or crashed */
			SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
						InvalidTransactionId);
			return false;
		}
	}

	/*
	 * By here, the inserting transaction has committed - have to check
	 * when...
	 */
	inSnapshot =
		XidInMVCCSnapshot(HeapTupleHeaderGetXmin(tuple), snapshot,
						  /* isXmax */ false,
						  ((tuple->t_infomask2 & HEAP_XMIN_DISTRIBUTED_SNAPSHOT_IGNORE) != 0),
						  &setDistributedSnapshotIgnore);
	if (setDistributedSnapshotIgnore)
	{
		tuple->t_infomask2 |= HEAP_XMIN_DISTRIBUTED_SNAPSHOT_IGNORE;
		markDirty(buffer, relation, tuple, /* isXmin */ true);
	}

	if (inSnapshot)
		return false;			/* treat as still in progress */

	if (tuple->t_infomask & HEAP_XMAX_INVALID)	/* xid invalid or aborted */
		return true;

	if (tuple->t_infomask & HEAP_IS_LOCKED)
		return true;

	if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
	{
		/* MultiXacts are currently only allowed to lock tuples */
		Assert(tuple->t_infomask & HEAP_IS_LOCKED);
		return true;
	}

	if (!(tuple->t_infomask & HEAP_XMAX_COMMITTED))
	{
		if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmax(tuple)))
		{
			if (HeapTupleHeaderGetCmax(tuple) >= snapshot->curcid)
				return true;	/* deleted after scan started */
			else
				return false;	/* deleted before scan started */
		}

		if (TransactionIdIsInProgress(HeapTupleHeaderGetXmax(tuple)))
			return true;

		if (!TransactionIdDidCommit(HeapTupleHeaderGetXmax(tuple)))
		{
			/* it must have aborted or crashed */
			SetHintBits(tuple, buffer, relation, HEAP_XMAX_INVALID,
						InvalidTransactionId);
			return true;
		}

		/* xmax transaction committed */
		SetHintBits(tuple, buffer, relation, HEAP_XMAX_COMMITTED,
					HeapTupleHeaderGetXmax(tuple));
	}

	/*
	 * OK, the deleting transaction committed too ... but when?
	 */
	inSnapshot =
			XidInMVCCSnapshot(HeapTupleHeaderGetXmax(tuple), snapshot,
							  /* isXmax */ true,
							  ((tuple->t_infomask2 & HEAP_XMAX_DISTRIBUTED_SNAPSHOT_IGNORE) != 0),
							  &setDistributedSnapshotIgnore);
	if (setDistributedSnapshotIgnore)
	{
		tuple->t_infomask2 |= HEAP_XMAX_DISTRIBUTED_SNAPSHOT_IGNORE;
		markDirty(buffer, relation, tuple, /* isXmin */ false);
	}

	if (inSnapshot)
		return true;			/* treat as still in progress */

	return false;
}

/*
 * HeapTupleSatisfiesVacuum
 *
 *	Determine the status of tuples for VACUUM purposes.  Here, what
 *	we mainly want to know is if a tuple is potentially visible to *any*
 *	running transaction.  If so, it can't be removed yet by VACUUM.
 *
 * OldestXmin is a cutoff XID (obtained from GetOldestXmin()).	Tuples
 * deleted by XIDs >= OldestXmin are deemed "recently dead"; they might
 * still be visible to some open transaction, so we can't remove them,
 * even if we see that the deleting transaction has committed.
 */
HTSV_Result
HeapTupleSatisfiesVacuum(Relation relation, HeapTupleHeader tuple, TransactionId OldestXmin,
						 Buffer buffer)
{
	/*
	 * Has inserting transaction committed?
	 *
	 * If the inserting transaction aborted, then the tuple was never visible
	 * to any other transaction, so we can delete it immediately.
	 */
	if (!(tuple->t_infomask & HEAP_XMIN_COMMITTED))
	{
		if (tuple->t_infomask & HEAP_XMIN_INVALID)
			return HEAPTUPLE_DEAD;
		else if (tuple->t_infomask & HEAP_MOVED_OFF)
		{
			TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

			if (TransactionIdIsCurrentTransactionId(xvac))
				return HEAPTUPLE_DELETE_IN_PROGRESS;
			if (TransactionIdIsInProgress(xvac))
				return HEAPTUPLE_DELETE_IN_PROGRESS;
			if (TransactionIdDidCommit(xvac))
			{
				SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
							InvalidTransactionId);
				return HEAPTUPLE_DEAD;
			}
			SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
						InvalidTransactionId);
		}
		else if (tuple->t_infomask & HEAP_MOVED_IN)
		{
			TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

			if (TransactionIdIsCurrentTransactionId(xvac))
				return HEAPTUPLE_INSERT_IN_PROGRESS;
			if (TransactionIdIsInProgress(xvac))
				return HEAPTUPLE_INSERT_IN_PROGRESS;
			if (TransactionIdDidCommit(xvac))
				SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
							InvalidTransactionId);
			else
			{
				SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
							InvalidTransactionId);
				return HEAPTUPLE_DEAD;
			}
		}
		else if (TransactionIdIsInProgress(HeapTupleHeaderGetXmin(tuple)))
		{
			if (tuple->t_infomask & HEAP_XMAX_INVALID)	/* xid invalid */
				return HEAPTUPLE_INSERT_IN_PROGRESS;
			if (tuple->t_infomask & HEAP_IS_LOCKED)
				return HEAPTUPLE_INSERT_IN_PROGRESS;
			/* inserted and then deleted by same xact */
			return HEAPTUPLE_DELETE_IN_PROGRESS;
		}
		else if (TransactionIdDidCommit(HeapTupleHeaderGetXmin(tuple)))
			SetHintBits(tuple, buffer, relation, HEAP_XMIN_COMMITTED,
						HeapTupleHeaderGetXmin(tuple));
		else
		{
			/*
			 * Not in Progress, Not Committed, so either Aborted or crashed
			 */
			SetHintBits(tuple, buffer, relation, HEAP_XMIN_INVALID,
						InvalidTransactionId);
			return HEAPTUPLE_DEAD;
		}

		/*
		 * At this point the xmin is known committed, but we might not have
		 * been able to set the hint bit yet; so we can no longer Assert that
		 * it's set.
		 */
	}

	/*
	 * Okay, the inserter committed, so it was good at some point.	Now what
	 * about the deleting transaction?
	 */
	if (tuple->t_infomask & HEAP_XMAX_INVALID)
		return HEAPTUPLE_LIVE;

	if (tuple->t_infomask & HEAP_IS_LOCKED)
	{
		/*
		 * "Deleting" xact really only locked it, so the tuple is live in any
		 * case.  However, we should make sure that either XMAX_COMMITTED or
		 * XMAX_INVALID gets set once the xact is gone, to reduce the costs of
		 * examining the tuple for future xacts.  Also, marking dead
		 * MultiXacts as invalid here provides defense against MultiXactId
		 * wraparound (see also comments in heap_freeze_tuple()).
		 */
		if (!(tuple->t_infomask & HEAP_XMAX_COMMITTED))
		{
			if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
			{
				if (MultiXactIdIsRunning(HeapTupleHeaderGetXmax(tuple)))
					return HEAPTUPLE_LIVE;
			}
			else
			{
				if (TransactionIdIsInProgress(HeapTupleHeaderGetXmax(tuple)))
					return HEAPTUPLE_LIVE;
			}

			/*
			 * We don't really care whether xmax did commit, abort or crash.
			 * We know that xmax did lock the tuple, but it did not and will
			 * never actually update it.
			 */
			SetHintBits(tuple, buffer, relation, HEAP_XMAX_INVALID,
						InvalidTransactionId);
		}
		return HEAPTUPLE_LIVE;
	}

	if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
	{
		/* MultiXacts are currently only allowed to lock tuples */
		Assert(tuple->t_infomask & HEAP_IS_LOCKED);
		return HEAPTUPLE_LIVE;
	}

	if (!(tuple->t_infomask & HEAP_XMAX_COMMITTED))
	{
		if (TransactionIdIsInProgress(HeapTupleHeaderGetXmax(tuple)))
			return HEAPTUPLE_DELETE_IN_PROGRESS;
		else if (TransactionIdDidCommit(HeapTupleHeaderGetXmax(tuple)))
			SetHintBits(tuple, buffer, relation, HEAP_XMAX_COMMITTED,
						HeapTupleHeaderGetXmax(tuple));
		else
		{
			/*
			 * Not in Progress, Not Committed, so either Aborted or crashed
			 */
			SetHintBits(tuple, buffer, relation, HEAP_XMAX_INVALID,
						InvalidTransactionId);
			return HEAPTUPLE_LIVE;
		}

		/*
		 * At this point the xmax is known committed, but we might not have
		 * been able to set the hint bit yet; so we can no longer Assert that
		 * it's set.
		 */
	}

	/*
	 * Deleter committed, but perhaps it was recent enough that some open
	 * transactions could still see the tuple.
	 */
	if (!TransactionIdPrecedes(HeapTupleHeaderGetXmax(tuple), OldestXmin))
		return HEAPTUPLE_RECENTLY_DEAD;

	/* Otherwise, it's dead and removable */
	return HEAPTUPLE_DEAD;
}


/*
 * GetTransactionSnapshot
 *		Get the appropriate snapshot for a new query in a transaction.
 *
 * The SerializableSnapshot is the first one taken in a transaction.
 * In serializable mode we just use that one throughout the transaction.
 * In read-committed mode, we take a new snapshot each time we are called.
 *
 * Note that the return value points at static storage that will be modified
 * by future calls and by CommandCounterIncrement().  Callers should copy
 * the result with CopySnapshot() if it is to be used very long.
 */
Snapshot
GetTransactionSnapshot(void)
{
	/* First call in transaction? */
	if (SerializableSnapshot == NULL)
	{
		SerializableSnapshot = GetSnapshotData(&SerializableSnapshotData, true);
		return SerializableSnapshot;
	}

	if (IsXactIsoLevelSerializable)
	{
		elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),"[Distributed Snapshot #%u] *Serializable Skip* (gxid = %u, '%s')",
			 (SerializableSnapshot == NULL ? 0 : SerializableSnapshot->distribSnapshotWithLocalMapping.header.distribSnapshotId),
			 getDistributedTransactionId(),
			 DtxContextToString(DistributedTransactionContext));

		UpdateSerializableCommandId();

		return SerializableSnapshot;
	}

	elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),"[Distributed Snapshot #%u] (gxid = %u, '%s')",
		 (LatestSnapshot == NULL ? 0 : LatestSnapshot->distribSnapshotWithLocalMapping.header.distribSnapshotId),
		 getDistributedTransactionId(),
		 DtxContextToString(DistributedTransactionContext));

	LatestSnapshot = GetSnapshotData(&LatestSnapshotData, false);

	return LatestSnapshot;
}

/*
 * GetLatestSnapshot
 *		Get a snapshot that is up-to-date as of the current instant,
 *		even if we are executing in SERIALIZABLE mode.
 */
Snapshot
GetLatestSnapshot(void)
{
	/* Should not be first call in transaction */
	if (SerializableSnapshot == NULL)
		elog(ERROR, "no snapshot has been set");

	LatestSnapshot = GetSnapshotData(&LatestSnapshotData, false);

	return LatestSnapshot;
}

/*
 * CopySnapshot
 *		Copy the given snapshot.
 *
 * The copy is palloc'd in the current memory context.
 */
Snapshot
CopySnapshot(Snapshot snapshot)
{
	Snapshot	newsnap;
	Size		subxipoff;
	Size		dsoff = 0;
	Size		size;

	if (!IsMVCCSnapshot(snapshot))
		return snapshot;

	/* We allocate any XID arrays needed in the same palloc block. */
	size = subxipoff = sizeof(SnapshotData) +
		snapshot->xcnt * sizeof(TransactionId);
	if (snapshot->subxcnt > 0)
		size += snapshot->subxcnt * sizeof(TransactionId);

	if (snapshot->haveDistribSnapshot &&
		snapshot->distribSnapshotWithLocalMapping.header.count > 0)
	{
		dsoff = size;
		size += snapshot->distribSnapshotWithLocalMapping.header.count *
			sizeof(DistributedSnapshotMapEntry);
	}

	newsnap = (Snapshot) palloc(size);
	memcpy(newsnap, snapshot, sizeof(SnapshotData));

	/* setup XID array */
	if (snapshot->xcnt > 0)
	{
		newsnap->xip = (TransactionId *) (newsnap + 1);
		memcpy(newsnap->xip, snapshot->xip,
			   snapshot->xcnt * sizeof(TransactionId));
	}
	else
		newsnap->xip = NULL;

	/* setup subXID array */
	if (snapshot->subxcnt > 0)
	{
		newsnap->subxip = (TransactionId *) ((char *) newsnap + subxipoff);
		memcpy(newsnap->subxip, snapshot->subxip,
			   snapshot->subxcnt * sizeof(TransactionId));
	}
	else
		newsnap->subxip = NULL;

	if (snapshot->haveDistribSnapshot &&
		snapshot->distribSnapshotWithLocalMapping.header.count > 0)
	{
		newsnap->distribSnapshotWithLocalMapping.inProgressEntryArray =
			(DistributedSnapshotMapEntry *) ((char *) newsnap + dsoff);
		memcpy(newsnap->distribSnapshotWithLocalMapping.inProgressEntryArray,
			   snapshot->distribSnapshotWithLocalMapping.inProgressEntryArray,
			   snapshot->distribSnapshotWithLocalMapping.header.count *
			   sizeof(DistributedSnapshotMapEntry));
	}
	else
	{
		newsnap->distribSnapshotWithLocalMapping.inProgressEntryArray = NULL;
	}

	return newsnap;
}

/*
 * FreeSnapshot
 *		Free a snapshot previously copied with CopySnapshot.
 *
 * This is currently identical to pfree, but is provided for cleanliness.
 *
 * Do *not* apply this to the results of GetTransactionSnapshot or
 * GetLatestSnapshot, since those are just static structs.
 */
void
FreeSnapshot(Snapshot snapshot)
{
	if (!IsMVCCSnapshot(snapshot))
		return;

	pfree(snapshot);
}

/*
 * FreeXactSnapshot
 *		Free snapshot(s) at end of transaction.
 */
void
FreeXactSnapshot(void)
{
	/*
	 * We do not free the xip arrays for the static snapshot structs; they
	 * will be reused soon. So this is now just a state change to prevent
	 * outside callers from accessing the snapshots.
	 */
	SerializableSnapshot = NULL;
	LatestSnapshot = NULL;
	ActiveSnapshot = NULL;		/* just for cleanliness */
}

/*
 * XidInMVCCSnapshot
 *		Is the given XID still-in-progress according to the distributed
 *      and local snapshots?
 */
static bool
XidInMVCCSnapshot(TransactionId xid, Snapshot snapshot, bool isXmax,
				  bool distributedSnapshotIgnore, bool *setDistributedSnapshotIgnore)
{
	Assert (setDistributedSnapshotIgnore != NULL);
	*setDistributedSnapshotIgnore = false;

	/*
	 * If we have a distributed snapshot, it takes precedence over the local
	 * snapshot since it covers the correct past view of in-progress distributed
	 * transactions and also the correct future view of in-progress distributed
	 * transactions that may yet arrive.
	 */
	if (snapshot->haveDistribSnapshot && !distributedSnapshotIgnore)
	{
		DistributedSnapshotCommitted	distributedSnapshotCommitted;

		/*
		 * First, check if this committed transaction is a distributed committed
		 * transaction and should be evaluated against the distributed snapshot
		 * instead.
		 */
		distributedSnapshotCommitted =
			DistributedSnapshotWithLocalMapping_CommittedTest(
				&snapshot->distribSnapshotWithLocalMapping,
				xid,
				isXmax);

		switch (distributedSnapshotCommitted)
		{
			case DISTRIBUTEDSNAPSHOT_COMMITTED_INPROGRESS:
				return true;

			case DISTRIBUTEDSNAPSHOT_COMMITTED_VISIBLE:
				return false;

			case DISTRIBUTEDSNAPSHOT_COMMITTED_IGNORE:
				/*
				 * We can safely skip both of these in the future for distributed
				 * snapshots.
				 */
				*setDistributedSnapshotIgnore = true;
				break;

			default:
				elog(FATAL, "Unrecognized distributed committed test result: %d",
					 (int) distributedSnapshotCommitted);
				break;
		}
	}

	return XidInMVCCSnapshot_Local(xid, snapshot, isXmax);
}

/*
 * XidInMVCCSnapshot_Local
 *		Is the given XID still-in-progress according to the local snapshot?
 *
 * Note: GetSnapshotData never stores either top xid or subxids of our own
 * backend into a snapshot, so these xids will not be reported as "running"
 * by this function.  This is OK for current uses, because we actually only
 * apply this for known-committed XIDs.
 */
static bool
XidInMVCCSnapshot_Local(TransactionId xid, Snapshot snapshot, bool isXmax)
{
	uint32		i;

	/*
	 * Make a quick range check to eliminate most XIDs without looking at the
	 * xip arrays.	Note that this is OK even if we convert a subxact XID to
	 * its parent below, because a subxact with XID < xmin has surely also got
	 * a parent with XID < xmin, while one with XID >= xmax must belong to a
	 * parent that was not yet committed at the time of this snapshot.
	 */

	/* Any xid < xmin is not in-progress */
	if (TransactionIdPrecedes(xid, snapshot->xmin))
		return false;
	/* Any xid >= xmax is in-progress */
	if (TransactionIdFollowsOrEquals(xid, snapshot->xmax))
		return true;

	/*
	 * If the snapshot contains full subxact data, the fastest way to check
	 * things is just to compare the given XID against both subxact XIDs and
	 * top-level XIDs.	If the snapshot overflowed, we have to use pg_subtrans
	 * to convert a subxact XID to its parent XID, but then we need only look
	 * at top-level XIDs not subxacts.
	 */
	if (snapshot->subxcnt >= 0)
	{
		/* full data, so search subxip */
		int32		j;

		for (j = 0; j < snapshot->subxcnt; j++)
		{
			if (TransactionIdEquals(xid, snapshot->subxip[j]))
				return true;
		}

		/* not there, fall through to search xip[] */
	}
	else
	{
		/* overflowed, so convert xid to top-level */
		xid = SubTransGetTopmostTransaction(xid);

		/*
		 * If xid was indeed a subxact, we might now have an xid < xmin, so
		 * recheck to avoid an array scan.	No point in rechecking xmax.
		 */
		if (TransactionIdPrecedes(xid, snapshot->xmin))
			return false;
	}

	for (i = 0; i < snapshot->xcnt; i++)
	{
		if (TransactionIdEquals(xid, snapshot->xip[i]))
			return true;
	}

	return false;
}

static char *TupleTransactionStatus_Name(TupleTransactionStatus status)
{
	switch (status)
	{
	case TupleTransactionStatus_None: 				return "None";
	case TupleTransactionStatus_Frozen: 			return "Frozen";
	case TupleTransactionStatus_HintCommitted: 		return "Hint-Committed";
	case TupleTransactionStatus_HintAborted: 		return "Hint-Aborted";
	case TupleTransactionStatus_CLogInProgress: 	return "CLog-In-Progress";
	case TupleTransactionStatus_CLogCommitted: 		return "CLog-Committed";
	case TupleTransactionStatus_CLogAborted:		return "CLog-Aborted";
	case TupleTransactionStatus_CLogSubCommitted:	return "CLog-Sub-Committed";
	default:
		return "Unknown";
	}
}

static char *TupleVisibilityStatus_Name(TupleVisibilityStatus status)
{
	switch (status)
	{
	case TupleVisibilityStatus_Unknown: 	return "Unknown";
	case TupleVisibilityStatus_InProgress: 	return "In-Progress";
	case TupleVisibilityStatus_Aborted: 	return "Aborted";
	case TupleVisibilityStatus_Past: 		return "Past";
	case TupleVisibilityStatus_Now: 		return "Now";
	default:
		return "Unknown";
	}
}

static TupleTransactionStatus GetTupleVisibilityCLogStatus(TransactionId xid)
{
	XidStatus xidStatus;
	XLogRecPtr lsn;

	xidStatus = TransactionIdGetStatus(xid, &lsn);
	switch (xidStatus)
	{
	case TRANSACTION_STATUS_IN_PROGRESS:	return TupleTransactionStatus_CLogInProgress;
	case TRANSACTION_STATUS_COMMITTED:		return TupleTransactionStatus_CLogCommitted;
	case TRANSACTION_STATUS_ABORTED:		return TupleTransactionStatus_CLogAborted;
	case TRANSACTION_STATUS_SUB_COMMITTED:	return TupleTransactionStatus_CLogSubCommitted;
	default:
		// Never gets here.  XidStatus is only 2-bits.
		return TupleTransactionStatus_None;
	}
}

void GetTupleVisibilitySummary(
	HeapTuple				tuple,
	TupleVisibilitySummary	*tupleVisibilitySummary)
{
	tupleVisibilitySummary->tid = tuple->t_self;
	tupleVisibilitySummary->infomask = tuple->t_data->t_infomask;
	tupleVisibilitySummary->infomask2 = tuple->t_data->t_infomask2;
	tupleVisibilitySummary->updateTid = tuple->t_data->t_ctid;

	tupleVisibilitySummary->xmin = HeapTupleHeaderGetXmin(tuple->t_data);
	if (!TransactionIdIsNormal(tupleVisibilitySummary->xmin))
	{
		if (tupleVisibilitySummary->xmin == FrozenTransactionId)
		{
			tupleVisibilitySummary->xminStatus = TupleTransactionStatus_Frozen;
		}
		else
		{
			tupleVisibilitySummary->xminStatus = TupleTransactionStatus_None;
		}
	}
	else if (tuple->t_data->t_infomask & HEAP_XMIN_COMMITTED)
	{
		tupleVisibilitySummary->xminStatus = TupleTransactionStatus_HintCommitted;
	}
	else if (tuple->t_data->t_infomask & HEAP_XMIN_INVALID)
	{
		tupleVisibilitySummary->xminStatus = TupleTransactionStatus_HintAborted;
	}
	else
	{
		tupleVisibilitySummary->xminStatus =
					GetTupleVisibilityCLogStatus(tupleVisibilitySummary->xmin);
	}
	tupleVisibilitySummary->xmax = HeapTupleHeaderGetXmax(tuple->t_data);
	if (!TransactionIdIsNormal(tupleVisibilitySummary->xmax))
	{
		if (tupleVisibilitySummary->xmax == FrozenTransactionId)
		{
			tupleVisibilitySummary->xmaxStatus = TupleTransactionStatus_Frozen;
		}
		else
		{
			tupleVisibilitySummary->xmaxStatus = TupleTransactionStatus_None;
		}
	}
	else if (tuple->t_data->t_infomask & HEAP_XMAX_COMMITTED)
	{
		tupleVisibilitySummary->xmaxStatus = TupleTransactionStatus_HintCommitted;
	}
	else if (tuple->t_data->t_infomask & HEAP_XMAX_INVALID)
	{
		tupleVisibilitySummary->xmaxStatus = TupleTransactionStatus_HintAborted;
	}
	else
	{
		tupleVisibilitySummary->xmaxStatus =
					GetTupleVisibilityCLogStatus(tupleVisibilitySummary->xmax);
	}

	tupleVisibilitySummary->cid =
					HeapTupleHeaderGetRawCommandId(tuple->t_data);

	/*
	 * Evaluate xmin and xmax status to produce overall visibility.
	 *
	 * UNDONE: Too simplistic?
	 */
	switch (tupleVisibilitySummary->xminStatus)
	{
	case TupleTransactionStatus_None:
	case TupleTransactionStatus_CLogSubCommitted:
		tupleVisibilitySummary->visibilityStatus = TupleVisibilityStatus_Unknown;
		break;

	case TupleTransactionStatus_CLogInProgress:
		tupleVisibilitySummary->visibilityStatus = TupleVisibilityStatus_InProgress;
		break;

	case TupleTransactionStatus_HintAborted:
	case TupleTransactionStatus_CLogAborted:
		tupleVisibilitySummary->visibilityStatus = TupleVisibilityStatus_Aborted;
		break;

	case TupleTransactionStatus_Frozen:
	case TupleTransactionStatus_HintCommitted:
	case TupleTransactionStatus_CLogCommitted:
		{
			switch (tupleVisibilitySummary->xmaxStatus)
			{
			case TupleTransactionStatus_None:
			case TupleTransactionStatus_Frozen:
			case TupleTransactionStatus_CLogInProgress:
			case TupleTransactionStatus_HintAborted:
			case TupleTransactionStatus_CLogAborted:
				tupleVisibilitySummary->visibilityStatus = TupleVisibilityStatus_Now;
				break;

			case TupleTransactionStatus_CLogSubCommitted:
				tupleVisibilitySummary->visibilityStatus = TupleVisibilityStatus_Unknown;
				break;

			case TupleTransactionStatus_HintCommitted:
			case TupleTransactionStatus_CLogCommitted:
				tupleVisibilitySummary->visibilityStatus = TupleVisibilityStatus_Past;
				break;

			default:
				elog(ERROR, "Unrecognized tuple transaction status: %d",
					 (int) tupleVisibilitySummary->xmaxStatus);
				tupleVisibilitySummary->visibilityStatus = TupleVisibilityStatus_Unknown;
				break;
			}
		}
		break;

	default:
		elog(ERROR, "Unrecognized tuple transaction status: %d",
			 (int) tupleVisibilitySummary->xminStatus);
		tupleVisibilitySummary->visibilityStatus = TupleVisibilityStatus_Unknown;
		break;
	}
}

static char *GetTupleVisibilityDistribId(
	TransactionId 				xid,
	TupleTransactionStatus      status)
{
	DistributedTransactionTimeStamp distribTimeStamp;
	DistributedTransactionId		distribXid;

	switch (status)
	{
	case TupleTransactionStatus_None:
	case TupleTransactionStatus_Frozen:
	case TupleTransactionStatus_CLogInProgress:
	case TupleTransactionStatus_HintAborted:
	case TupleTransactionStatus_CLogAborted:
	case TupleTransactionStatus_CLogSubCommitted:
		return NULL;

	case TupleTransactionStatus_HintCommitted:
	case TupleTransactionStatus_CLogCommitted:
		if (DistributedLog_CommittedCheck(
										xid,
										&distribTimeStamp,
										&distribXid))
		{
			char	*distribId;

			distribId = palloc(TMGIDSIZE);
			sprintf(distribId, "%u-%.10u",
					distribTimeStamp, distribXid);
			return distribId;
		}
		else
		{
			return pstrdup("(info not avail)");
		}
		break;

	default:
		elog(ERROR, "Unrecognized tuple transaction status: %d",
			 (int) status);
		return NULL;
	}
}

static void TupleVisibilityAddFlagName(
	StringInfoData *buf,

	int16			rawFlag,

	char			*flagName,

	bool			*atLeastOne)
{
	if (rawFlag != 0)
	{
		if (*atLeastOne)
		{
			appendStringInfo(buf, ", ");
		}
		appendStringInfo(buf, "%s", flagName);
		*atLeastOne = true;
	}
}

static char *GetTupleVisibilityInfoMaskSet(
	int16 				infomask,

	int16				infomask2)
{
	StringInfoData buf;

	bool atLeastOne;

	initStringInfo(&buf);
	appendStringInfo(&buf, "{");
	atLeastOne = false;

	TupleVisibilityAddFlagName(&buf, infomask & HEAP_COMBOCID, "HEAP_COMBOCID", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_XMAX_EXCL_LOCK, "HEAP_XMAX_EXCL_LOCK", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_XMAX_SHARED_LOCK, "HEAP_XMAX_SHARED_LOCK", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_XMIN_COMMITTED, "HEAP_XMIN_COMMITTED", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_XMIN_INVALID, "HEAP_XMIN_INVALID", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_XMAX_COMMITTED, "HEAP_XMAX_COMMITTED", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_XMAX_INVALID, "HEAP_XMAX_INVALID", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_XMAX_IS_MULTI, "HEAP_XMAX_IS_MULTI", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_UPDATED, "HEAP_UPDATED", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_MOVED_OFF, "HEAP_MOVED_OFF", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_MOVED_IN, "HEAP_MOVED_IN", &atLeastOne);

	TupleVisibilityAddFlagName(&buf, infomask2 & HEAP_XMIN_DISTRIBUTED_SNAPSHOT_IGNORE, "HEAP_XMIN_DISTRIBUTED_SNAPSHOT_IGNORE", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask2 & HEAP_XMAX_DISTRIBUTED_SNAPSHOT_IGNORE, "HEAP_XMAX_DISTRIBUTED_SNAPSHOT_IGNORE", &atLeastOne);

	appendStringInfo(&buf, "}");
	return buf.data;
}

// 0  gp_tid                    			TIDOID
// 1  gp_xmin                  			INT4OID
// 2  gp_xmin_status        			TEXTOID
// 3  gp_xmin_commit_distrib_id		TEXTOID
// 4  gp_xmax		          		INT4OID
// 5  gp_xmax_status       			TEXTOID
// 6  gp_xmax_distrib_id    			TEXTOID
// 7  gp_command_id	    			INT4OID
// 8  gp_infomask    	   			TEXTOID
// 9  gp_update_tid         			TIDOID
// 10 gp_visibility             			TEXTOID

void GetTupleVisibilitySummaryDatums(
	Datum		*values,
	bool		*nulls,
	TupleVisibilitySummary	*tupleVisibilitySummary)
{
	char *xminDistribId;
	char *xmaxDistribId;
	char *infoMaskSet;

	values[0] = ItemPointerGetDatum(&tupleVisibilitySummary->tid);
	values[1] = Int32GetDatum((int32)tupleVisibilitySummary->xmin);
	values[2] =
		DirectFunctionCall1(textin,
							CStringGetDatum(
								TupleTransactionStatus_Name(
											tupleVisibilitySummary->xminStatus)));
	xminDistribId = GetTupleVisibilityDistribId(
									tupleVisibilitySummary->xmin,
									tupleVisibilitySummary->xminStatus);
	if (xminDistribId != NULL)
	{
		values[3] =
			DirectFunctionCall1(textin,
								CStringGetDatum(
										xminDistribId));
		pfree(xminDistribId);
	}
	else
	{
		nulls[3] = true;
	}
	values[4] = Int32GetDatum((int32)tupleVisibilitySummary->xmax);
	values[5] =
		DirectFunctionCall1(textin,
							CStringGetDatum(
								TupleTransactionStatus_Name(
											tupleVisibilitySummary->xmaxStatus)));
	xmaxDistribId = GetTupleVisibilityDistribId(
									tupleVisibilitySummary->xmax,
									tupleVisibilitySummary->xmaxStatus);
	if (xmaxDistribId != NULL)
	{
		values[6] =
			DirectFunctionCall1(textin,
								CStringGetDatum(
										xmaxDistribId));
		pfree(xmaxDistribId);
	}
	else
	{
		nulls[6] = true;
	}
	values[7] = Int32GetDatum((int32)tupleVisibilitySummary->cid);
	infoMaskSet = GetTupleVisibilityInfoMaskSet(
									tupleVisibilitySummary->infomask,
									tupleVisibilitySummary->infomask2);
	values[8] =
		DirectFunctionCall1(textin,
							CStringGetDatum(
									infoMaskSet));
	pfree(infoMaskSet);
	values[9] = ItemPointerGetDatum(&tupleVisibilitySummary->updateTid);
	values[10] =
		DirectFunctionCall1(textin,
							CStringGetDatum(
								TupleVisibilityStatus_Name(
											tupleVisibilitySummary->visibilityStatus)));
}

char *GetTupleVisibilitySummaryString(
	TupleVisibilitySummary	*tupleVisibilitySummary)
{
	StringInfoData buf;

	char *xminDistribId;
	char *xmaxDistribId;
	char *infoMaskSet;

	initStringInfo(&buf);
	appendStringInfo(&buf, "tid %s",
					 ItemPointerToString(&tupleVisibilitySummary->tid));
	appendStringInfo(&buf, ", xmin %u",
					 tupleVisibilitySummary->xmin);
	appendStringInfo(&buf, ", xmin_status '%s'",
					 TupleTransactionStatus_Name(
							tupleVisibilitySummary->xminStatus));

	xminDistribId = GetTupleVisibilityDistribId(
									tupleVisibilitySummary->xmin,
									tupleVisibilitySummary->xminStatus);
	if (xminDistribId != NULL)
	{
		appendStringInfo(&buf, ", xmin_commit_distrib_id '%s'",
						 xminDistribId);
		pfree(xminDistribId);
	}
	appendStringInfo(&buf, ", xmax %u",
					 tupleVisibilitySummary->xmax);
	appendStringInfo(&buf, ", xmax_status '%s'",
					 TupleTransactionStatus_Name(
							tupleVisibilitySummary->xmaxStatus));

	xmaxDistribId = GetTupleVisibilityDistribId(
									tupleVisibilitySummary->xmax,
									tupleVisibilitySummary->xmaxStatus);
	if (xmaxDistribId != NULL)
	{
		appendStringInfo(&buf, ", xmax_commit_distrib_id '%s'",
						 xmaxDistribId);
		pfree(xmaxDistribId);
	}
	appendStringInfo(&buf, ", command_id %u",
					 tupleVisibilitySummary->cid);
	infoMaskSet = GetTupleVisibilityInfoMaskSet(
									tupleVisibilitySummary->infomask,
									tupleVisibilitySummary->infomask2);
	appendStringInfo(&buf, ", infomask '%s'",
					 infoMaskSet);
	pfree(infoMaskSet);
	appendStringInfo(&buf, ", update_tid %s",
					 ItemPointerToString(&tupleVisibilitySummary->updateTid));
	appendStringInfo(&buf, ", visibility '%s'",
					 TupleVisibilityStatus_Name(
											tupleVisibilitySummary->visibilityStatus));

	return buf.data;
}
