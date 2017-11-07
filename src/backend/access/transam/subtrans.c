/*-------------------------------------------------------------------------
 *
 * subtrans.c
 *		PostgreSQL subtransaction-log manager
 *
 * The pg_subtrans manager is a pg_clog-like manager that stores the parent
 * transaction Id for each transaction.  It is a fundamental part of the
 * nested transactions implementation.	A main transaction has a parent
 * of InvalidTransactionId, and each subtransaction has its immediate parent.
 * The tree can easily be walked from child to parent, but not in the
 * opposite direction.
 *
 * This code is based on clog.c, but the robustness requirements
 * are completely different from pg_clog, because we only need to remember
 * pg_subtrans information for currently-open transactions.  Thus, there is
 * no need to preserve data over a crash and restart.
 *
 * There are no XLOG interactions since we do not care about preserving
 * data across crashes.  During database startup, we simply force the
 * currently-active page of SUBTRANS to zeroes.
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/backend/access/transam/subtrans.c,v 1.23 2008/08/01 13:16:08 alvherre Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/slru.h"
#include "access/subtrans.h"
#include "access/transam.h"
#include "pg_trace.h"
#include "utils/snapmgr.h"
#include "utils/tqual.h"
#include "cdb/cdbpersistentstore.h"


/*
 * Defines for SubTrans page sizes.  A page is the same BLCKSZ as is used
 * everywhere else in Postgres.
 *
 * Note: because TransactionIds are 32 bits and wrap around at 0xFFFFFFFF,
 * SubTrans page numbering also wraps around at
 * 0xFFFFFFFF/SUBTRANS_XACTS_PER_PAGE, and segment numbering at
 * 0xFFFFFFFF/SUBTRANS_XACTS_PER_PAGE/SLRU_SEGMENTS_PER_PAGE.  We need take no
 * explicit notice of that fact in this module, except when comparing segment
 * and page numbers in TruncateSUBTRANS (see SubTransPagePrecedes).
 */

/* We need eight bytes per xact */
#define SUBTRANS_XACTS_PER_PAGE (BLCKSZ / sizeof(SubTransData))

#define TransactionIdToPage(xid) ((xid) / (uint32) SUBTRANS_XACTS_PER_PAGE)
#define TransactionIdToEntry(xid) ((xid) % (uint32) SUBTRANS_XACTS_PER_PAGE)


/*
 * Link to shared-memory data structures for SUBTRANS control
 */
static SlruCtlData SubTransCtlData;

#define SubTransCtl  (&SubTransCtlData)


static int	ZeroSUBTRANSPage(int pageno);
static bool SubTransPagePrecedes(int page1, int page2);

static void
SubTransGetData(TransactionId xid, SubTransData* subData)
{
	MIRRORED_LOCK_DECLARE;

	int			pageno = TransactionIdToPage(xid);
	int			entryno = TransactionIdToEntry(xid);
	int			slotno;
	SubTransData *ptr;

	/* Can't ask about stuff that might not be around anymore */
	Assert(TransactionIdFollowsOrEquals(xid, TransactionXmin));

	/* Bootstrap and frozen XIDs have no parent and itself as topMostParent */
	if (!TransactionIdIsNormal(xid))
	{
		subData->parent = InvalidTransactionId;
		subData->topMostParent = xid;
		return;
	}

	MIRRORED_LOCK;

	/* lock is acquired by SimpleLruReadPage_ReadOnly */

	slotno = SimpleLruReadPage_ReadOnly(SubTransCtl, pageno, xid, NULL);
	ptr = (SubTransData *) SubTransCtl->shared->page_buffer[slotno];
	ptr += entryno;

	subData->parent = ptr->parent;
	subData->topMostParent = ptr->topMostParent;
	if ( subData->topMostParent == InvalidTransactionId )
	{
		/* Here means parent is Main XID, hence set parent itself as topMostParent */
		subData->topMostParent = xid;
	}

	LWLockRelease(SubtransControlLock);

	MIRRORED_UNLOCK;

	return;
}

/*
 * Record the parent of a subtransaction in the subtrans log.
 */
void
SubTransSetParent(TransactionId xid, TransactionId parent)
{
	MIRRORED_LOCK_DECLARE;

	int			pageno = TransactionIdToPage(xid);
	int			entryno = TransactionIdToEntry(xid);
	int			slotno;
	SubTransData *ptr;
	SubTransData subData;

	/*
	 * Main Xact has parent and topMostParent as InvalidTransactionId
	 */
	if ( parent != InvalidTransactionId )
	{
		/* Get the topMostParent for Parent */
		SubTransGetData(parent, &subData);
	}
	else
	{
		subData.topMostParent = InvalidTransactionId;
	}

	MIRRORED_LOCK;

	LWLockAcquire(SubtransControlLock, LW_EXCLUSIVE);

	slotno = SimpleLruReadPage(SubTransCtl, pageno, true, xid);
	ptr = (SubTransData *) SubTransCtl->shared->page_buffer[slotno];
	ptr += entryno;

	/* Current state should be 0 */
	Assert(ptr->parent == InvalidTransactionId);
	Assert(ptr->topMostParent == InvalidTransactionId);

	ptr->parent = parent;
	ptr->topMostParent = subData.topMostParent;

	SubTransCtl->shared->page_dirty[slotno] = true;

	LWLockRelease(SubtransControlLock);

	MIRRORED_UNLOCK;
}

/*
 * Interrogate the parent of a transaction in the subtrans log.
 */
TransactionId
SubTransGetParent(TransactionId xid)
{
	SubTransData subData;
	SubTransGetData(xid, &subData);

	return subData.parent;
}

/*
 * SubTransGetTopmostTransaction
 *
 * Returns the topmost transaction of the given transaction id.
 */
TransactionId
SubTransGetTopmostTransaction(TransactionId xid)
{
	Assert(TransactionIdIsValid(xid));
	SubTransData subData;
	SubTransGetData(xid, &subData);

	Assert(TransactionIdIsValid(subData.topMostParent));
	return subData.topMostParent;
}

/*
 * Initialization of shared memory for SUBTRANS
 */
Size
SUBTRANSShmemSize(void)
{
	return SimpleLruShmemSize(NUM_SUBTRANS_BUFFERS, 0);
}

void
SUBTRANSShmemInit(void)
{
	SubTransCtl->PagePrecedes = SubTransPagePrecedes;
	SimpleLruInit(SubTransCtl, "SUBTRANS Ctl", NUM_SUBTRANS_BUFFERS, 0,
				  SubtransControlLock, SUBTRANS_DIR);
	/* Override default assumption that writes should be fsync'd */
	SubTransCtl->do_fsync = false;
}

/*
 * This func must be called ONCE on system install.  It creates
 * the initial SUBTRANS segment.  (The SUBTRANS directory is assumed to
 * have been created by the initdb shell script, and SUBTRANSShmemInit
 * must have been called already.)
 *
 * Note: it's not really necessary to create the initial segment now,
 * since slru.c would create it on first write anyway.	But we may as well
 * do it to be sure the directory is set up correctly.
 */
void
BootStrapSUBTRANS(void)
{
	MIRRORED_LOCK_DECLARE;

	int			slotno;

	MIRRORED_LOCK;

	LWLockAcquire(SubtransControlLock, LW_EXCLUSIVE);

	/* Create and zero the first page of the subtrans log */
	slotno = ZeroSUBTRANSPage(0);

	/* Make sure it's written out */
	SimpleLruWritePage(SubTransCtl, slotno, NULL);
	Assert(!SubTransCtl->shared->page_dirty[slotno]);

	LWLockRelease(SubtransControlLock);

	MIRRORED_UNLOCK;
}

/*
 * Initialize (or reinitialize) a page of SUBTRANS to zeroes.
 *
 * The page is not actually written, just set up in shared memory.
 * The slot number of the new page is returned.
 *
 * Control lock must be held at entry, and will be held at exit.
 */
static int
ZeroSUBTRANSPage(int pageno)
{
	MIRRORED_LOCK_DECLARE;

	int result;

	MIRRORED_LOCK;

	result = SimpleLruZeroPage(SubTransCtl, pageno);

	MIRRORED_UNLOCK;

	return result;
}

/*
 * This must be called ONCE during postmaster or standalone-backend startup,
 * after StartupXLOG has initialized ShmemVariableCache->nextXid.
 *
 * oldestActiveXID is the oldest XID of any prepared transaction, or nextXid
 * if there are none.
 */
void
StartupSUBTRANS(TransactionId oldestActiveXID)
{
	MIRRORED_LOCK_DECLARE;

	int			startPage;
	int			endPage;

	/*
	 * Since we don't expect pg_subtrans to be valid across crashes, we
	 * initialize the currently-active page(s) to zeroes during startup.
	 * Whenever we advance into a new page, ExtendSUBTRANS will likewise zero
	 * the new page without regard to whatever was previously on disk.
	 */

	MIRRORED_LOCK;

	LWLockAcquire(SubtransControlLock, LW_EXCLUSIVE);

	startPage = TransactionIdToPage(oldestActiveXID);
	endPage = TransactionIdToPage(ShmemVariableCache->nextXid);

	while (startPage != endPage)
	{
		(void) ZeroSUBTRANSPage(startPage);
		startPage++;
	}
	(void) ZeroSUBTRANSPage(startPage);

	LWLockRelease(SubtransControlLock);

	MIRRORED_UNLOCK;
}

/*
 * This must be called ONCE during postmaster or standalone-backend shutdown
 */
void
ShutdownSUBTRANS(void)
{
	MIRRORED_LOCK_DECLARE;

	MIRRORED_LOCK;

	/*
	 * Flush dirty SUBTRANS pages to disk
	 *
	 * This is not actually necessary from a correctness point of view. We do
	 * it merely as a debugging aid.
	 */
	TRACE_POSTGRESQL_SUBTRANS_CHECKPOINT_START(false);
	SimpleLruFlush(SubTransCtl, false);

	MIRRORED_UNLOCK;
	TRACE_POSTGRESQL_SUBTRANS_CHECKPOINT_DONE(false);
}

/*
 * Perform a checkpoint --- either during shutdown, or on-the-fly
 */
void
CheckPointSUBTRANS(void)
{
	MIRRORED_LOCK_DECLARE;

	MIRRORED_LOCK;

	/*
	 * Flush dirty SUBTRANS pages to disk
	 *
	 * This is not actually necessary from a correctness point of view. We do
	 * it merely to improve the odds that writing of dirty pages is done by
	 * the checkpoint process and not by backends.
	 */
	TRACE_POSTGRESQL_SUBTRANS_CHECKPOINT_START(true);
	SimpleLruFlush(SubTransCtl, true);

	MIRRORED_UNLOCK;
	TRACE_POSTGRESQL_SUBTRANS_CHECKPOINT_DONE(true);
}


/*
 * Make sure that SUBTRANS has room for a newly-allocated XID.
 *
 * NB: this is called while holding XidGenLock.  We want it to be very fast
 * most of the time; even when it's not so fast, no actual I/O need happen
 * unless we're forced to write out a dirty subtrans page to make room
 * in shared memory.
 */
void
ExtendSUBTRANS(TransactionId newestXact)
{
	int			pageno;

	/*
	 * Caller must have already taken mirrored lock shared.
	 */

	/*
	 * No work except at first XID of a page.  But beware: just after
	 * wraparound, the first XID of page zero is FirstNormalTransactionId.
	 */
	if (TransactionIdToEntry(newestXact) != 0 &&
		!TransactionIdEquals(newestXact, FirstNormalTransactionId))
		return;

	pageno = TransactionIdToPage(newestXact);

	LWLockAcquire(SubtransControlLock, LW_EXCLUSIVE);

	/* Zero the page */
	ZeroSUBTRANSPage(pageno);

	LWLockRelease(SubtransControlLock);
}


/*
 * Remove all SUBTRANS segments before the one holding the passed transaction ID
 *
 * This is normally called during checkpoint, with oldestXact being the
 * oldest TransactionXmin of any running transaction.
 */
void
TruncateSUBTRANS(TransactionId oldestXact)
{
	MIRRORED_LOCK_DECLARE;

	int			cutoffPage;

	/*
	 * The cutoff point is the start of the segment containing oldestXact. We
	 * pass the *page* containing oldestXact to SimpleLruTruncate.
	 */
	cutoffPage = TransactionIdToPage(oldestXact);

	MIRRORED_LOCK;

	SimpleLruTruncate(SubTransCtl, cutoffPage);

	MIRRORED_UNLOCK;
}


/*
 * Decide which of two SUBTRANS page numbers is "older" for truncation purposes.
 *
 * We need to use comparison of TransactionIds here in order to do the right
 * thing with wraparound XID arithmetic.  However, if we are asked about
 * page number zero, we don't want to hand InvalidTransactionId to
 * TransactionIdPrecedes: it'll get weird about permanent xact IDs.  So,
 * offset both xids by FirstNormalTransactionId to avoid that.
 */
static bool
SubTransPagePrecedes(int page1, int page2)
{
	TransactionId xid1;
	TransactionId xid2;

	xid1 = ((uint32) page1) * SUBTRANS_XACTS_PER_PAGE;
	xid1 += FirstNormalTransactionId;
	xid2 = ((uint32) page2) * SUBTRANS_XACTS_PER_PAGE;
	xid2 += FirstNormalTransactionId;

	return TransactionIdPrecedes(xid1, xid2);
}
