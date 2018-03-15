/*-------------------------------------------------------------------------
 *
 * distributedlog.c
 *     A GP parallel log to the Postgres clog that records the full distributed
 * xid information for each local transaction id.
 *
 * It is used to determine if the committed xid for a transaction we want to
 * determine the visibility of is for a distributed transaction or a
 * local transaction.
 *
 * By default, entries in the SLRU (Simple LRU) module used to implement this
 * log will be set to zero.  A non-zero entry indicates a committed distributed
 * transaction.
 *
 * We must persist this log and the DTM does reuse the DistributedTransactionId
 * between restarts, so we will be storing the upper half of the whole
 * distributed transaction identifier -- the timestamp -- also so we can
 * be sure which distributed transaction we are looking at.
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/transam/distributedlog.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/distributedlog.h"
#include "access/slru.h"
#include "access/transam.h"
#include "storage/shmem.h"
#include "utils/guc.h"

/* We need 8 bytes per xact */
#define ENTRIES_PER_PAGE (BLCKSZ / sizeof(DistributedLogEntry))

/*
 * XXX: This should match the value in slru.c. It's only used to decide when
 * to try truncating the log, so it's not critical, but if it doesn't match,
 * we'll try to truncate the log more often than necessary, or won't truncate
 * it as often as we could.
 */
#define SLRU_PAGES_PER_SEGMENT	32

#define TransactionIdToPage(localXid) ((localXid) / (TransactionId) ENTRIES_PER_PAGE)
#define TransactionIdToEntry(localXid) ((localXid) % (TransactionId) ENTRIES_PER_PAGE)

static TransactionId
AdvanceTransactionIdToNextPage(TransactionId xid)
{
	/* Advance to next page. */
	xid += ENTRIES_PER_PAGE;

	/* Retreat to beginning of the page */
	xid -= (xid % ENTRIES_PER_PAGE);

	/* Skip over the special XIDs */
	while (xid < FirstNormalTransactionId)
		xid++;

	return xid;
}


/*
 * Link to shared-memory data structures for DistributedLog control
 */
static SlruCtlData DistributedLogCtlData;

#define DistributedLogCtl (&DistributedLogCtlData)

typedef struct DistributedLogShmem
{
	/*
	 * Oldest local XID that is still visible to some distributed snapshot.
	 *
	 * This is initialized by DistributedLog_InitOldestXmin() after
	 * postmaster startup, and advanced whenever we receive a new
	 * distributed snapshot from the QD (or in the QD itself, whenever
	 * we compute a new snapshot).
	 */
	TransactionId	oldestXmin;

} DistributedLogShmem;

static DistributedLogShmem *DistributedLogShared = NULL;

static void DistributedLog_SetCommitted(TransactionId localXid,
							DistributedTransactionTimeStamp dtxStartTime,
							DistributedTransactionId distribXid,
							bool isRedo);
static int	DistributedLog_ZeroPage(int page, bool writeXlog);
static void DistributedLog_Truncate(void);
static bool DistributedLog_PagePrecedes(int page1, int page2);
static void DistributedLog_WriteZeroPageXlogRec(int page);
static void DistributedLog_WriteTruncateXlogRec(int page);

/*
 * Initialize the value for oldest local XID that might still be visible
 * to a distributed snapshot.
 *
 * This gets called once, after postmaster startup. We scan the
 * distributed log, starting from the smallest datfrozenxid, until
 * we find a page that exists.
 *
 * The caller is expected to hold DistributedLogControlLock on entry.
 */
static void
DistributedLog_InitOldestXmin(TransactionId oldestLocalXmin)
{
	TransactionId oldestXmin;

	if (DistributedLogShared->oldestXmin != InvalidTransactionId)
	{
		/* Already initialized. */
		return;
	}

	/*
	 * Start scanning from oldest datfrozenxid, until we find a
	 * valid page.
	 */
	oldestXmin = GetTransactionIdLimit();
	for (;;)
	{
		int			page = TransactionIdToPage(oldestXmin);
		TransactionId xid;

		if (SimpleLruPageExists(DistributedLogCtl, page))
		{
			/* Found the beginning of valid distributedlog */
			break;
		}

		/* Advance to the first XID on the next page */
		xid = AdvanceTransactionIdToNextPage(oldestXmin);

		/* but don't go past oldestLocalXmin */
		if (TransactionIdFollows(xid, oldestLocalXmin))
		{
			oldestXmin = oldestLocalXmin;
			break;
		}

		oldestXmin = xid;
	}

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "Initialized oldestxmin to %u", oldestXmin);

	DistributedLogShared->oldestXmin = oldestXmin;
}

/*
 * Advance the "oldest xmin" among distributed snapshots.
 *
 * We track an "oldest xmin" value, which is the oldest XID that is still
 * considered visible by any distributed snapshot in the cluster. The
 * value is a local TransactionId, not a DistributedTransactionId. But
 * it takes into account any snapshots that might still be active in the
 * QD node, even if there are no processes belonging to that distributed
 * transaction running in this segment at the moment.
 *
 * Call this function in the QE, whenever a new distributed snapshot is
 * received from the QD. Pass the 'distribTransactionTimeStamp' and
 * 'xminAllDistributedSnapshots' values from the DistributedSnapshot.
 * 'oldestLocalXmin' is the oldest xmin that is still visible according
 * to local snapshots. (That is the upper bound of how far we can advance
 * the oldest xmin)
 *
 * As a courtesy to callers, this function also returns the new "oldest
 * xmin" value (same as old value, if it was not advanced), just like
 * DistributedLog_GetOldestXmin() would.
 *
 */
TransactionId
DistributedLog_AdvanceOldestXmin(TransactionId oldestLocalXmin,
								 DistributedTransactionTimeStamp distribTransactionTimeStamp,
								 DistributedTransactionId xminAllDistributedSnapshots)
{
	TransactionId oldestXmin;
	int			currPage;
	int			slotno;
	DistributedLogEntry *entries = NULL;
	TransactionId oldOldestXmin;

	if (!TransactionIdIsNormal(oldestLocalXmin))
		elog(ERROR, "invalid oldest xmin: %u", oldestLocalXmin);

	LWLockAcquire(DistributedLogControlLock, LW_EXCLUSIVE);

	oldestXmin = oldOldestXmin = DistributedLogShared->oldestXmin;

	if (oldestXmin == InvalidTransactionId)
	{
		/*
		 * If we haven't seen any distributed snapshots from the QD yet,
		 * initialize oldest xmin to the oldest datfrozenxid in this segment.
		 *
		 * This might be quite far behind the actual value, so on the first
		 * call, the loop below can wind through millions of XIDs.
		 * Fortunately, that only happens on the first call. But if that
		 * ever becomes a problem, we could persist the value across
		 * server restarts e.g. in the checkpoint record.
		 *
		 * We must release DistributedLogControlLock first, to avoid deadlock.
		 * (GetNewTransactionId() can call DistributedLog_Truncate(), while
		 * already holding XidGenLock)
		 */
		DistributedLog_InitOldestXmin(oldestLocalXmin);
		oldestXmin = DistributedLogShared->oldestXmin;
	}

	/* sanity check, this shouldn't happen... */
	if (TransactionIdFollows(oldestXmin, oldestLocalXmin))
		elog(ERROR, "local snapshot's xmin is older than recorded distributed oldestxmin");

	currPage = -1;
	while (!TransactionIdEquals(oldestXmin, oldestLocalXmin))
	{
		int			page = TransactionIdToPage(oldestXmin);
		int			entryno = TransactionIdToEntry(oldestXmin);
		DistributedLogEntry *ptr;

		if (page != currPage)
		{
			slotno = SimpleLruReadPage(DistributedLogCtl, page, true, oldestXmin);
			currPage = page;
			entries = (DistributedLogEntry *) DistributedLogCtl->shared->page_buffer[slotno];
		}

		ptr = &entries[entryno];

		/*
		 * If this XID is already visible to all distributed snapshots, we can
		 * advance past it. Otherwise stop here. (Local-only transactions will
		 * have zeros in distribXid and distribTimeStamp; this test will also
		 * skip over those.)
		 */
		if (ptr->distribTimeStamp != distribTransactionTimeStamp ||
			TransactionIdPrecedes(ptr->distribXid, xminAllDistributedSnapshots))
		{
			TransactionIdAdvance(oldestXmin);
		}
		else
			break;
	}

	DistributedLogShared->oldestXmin = oldestXmin;

	/*
	 * Done. But if we crossed an SLRU segment boundary, we can now remove
	 * some old segments.
	 */
	if (oldOldestXmin % (ENTRIES_PER_PAGE * SLRU_PAGES_PER_SEGMENT) !=
		oldestXmin % (ENTRIES_PER_PAGE * SLRU_PAGES_PER_SEGMENT))
	{
		DistributedLog_Truncate();
	}

	LWLockRelease(DistributedLogControlLock);

	return oldestXmin;
}

/*
 * Return the "oldest xmin" among distributed snapshots.
 */
TransactionId
DistributedLog_GetOldestXmin(TransactionId oldestLocalXmin)
{
	TransactionId result;

	LWLockAcquire(DistributedLogControlLock, LW_EXCLUSIVE);
	result = DistributedLogShared->oldestXmin;

	if (result == InvalidTransactionId)
	{
		/*
		 * If we haven't "seen" any distributed snapshots from the QD yet, initialize
		 * oldest xmin based on how far back the distributed log reaches.
		 */
		DistributedLog_InitOldestXmin(oldestLocalXmin);
		result = DistributedLogShared->oldestXmin;
	}
	LWLockRelease(DistributedLogControlLock);

	Assert(TransactionIdFollowsOrEquals(oldestLocalXmin, result));

	return result;
}

/*
 * Record that a distributed transaction and its possible sub-transactions
 * committed, in the distributed log.
 */
void
DistributedLog_SetCommittedTree(TransactionId xid, int nxids, TransactionId *xids,
								DistributedTransactionTimeStamp	distribTimeStamp,
								DistributedTransactionId distribXid,
								bool isRedo)
{
	int			i;

	/*
	 * GPDB_84_MERGE_FIXME: This is a naive implementation, not very efficient.
	 * Should update the list of transaction one distributed clog page at a time.
	 * Similar to how we do for the clog now, since commit 06da3c570.
	 */
	DistributedLog_SetCommitted(xid,
								distribTimeStamp,
								distribXid,
								isRedo);
	for (i = 0; i < nxids; i++)
	{
		DistributedLog_SetCommitted(xids[i],
									distribTimeStamp,
									distribXid,
									isRedo);
	}
}


/*
 * Record that a distributed transaction committed in the distributed log.
 */
static void
DistributedLog_SetCommitted(
	TransactionId 						localXid,
	DistributedTransactionTimeStamp		distribTimeStamp,
	DistributedTransactionId 			distribXid,
	bool								isRedo)
{
	Assert(TransactionIdIsValid(localXid));

	int			page = TransactionIdToPage(localXid);
	int			entryno = TransactionIdToEntry(localXid);
	int			slotno;

	DistributedLogEntry *ptr;

	bool alreadyThere = false;

	LWLockAcquire(DistributedLogControlLock, LW_EXCLUSIVE);

	if (isRedo)
	{
		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "DistributedLog_SetCommitted check if page %d is present",
			 page);
		if (!SimpleLruPageExists(DistributedLogCtl, page))
		{
			DistributedLog_ZeroPage(page, /* writeXLog */ false);
			elog((Debug_print_full_dtm ? LOG : DEBUG5),
				 "DistributedLog_SetCommitted zeroed page %d",
				 page);
		}
	}

	slotno = SimpleLruReadPage(DistributedLogCtl, page, true, localXid);
	ptr = (DistributedLogEntry *) DistributedLogCtl->shared->page_buffer[slotno];
	ptr += entryno;

	if (ptr->distribTimeStamp != 0 || ptr->distribXid != 0)
	{
		if (ptr->distribTimeStamp != distribTimeStamp)
			elog(ERROR,
			     "Current distributed timestamp = %u does not match input timestamp = %u for local xid = %u in distributed log (page = %d, entryno = %d)",
			     ptr->distribTimeStamp, distribTimeStamp, localXid, page, entryno);

		if (ptr->distribXid != distribXid)
			elog(ERROR,
			     "Current distributed xid = %u does not match input distributed xid = %u for local xid = %u in distributed log (page = %d, entryno = %d)",
			     ptr->distribXid, distribXid, localXid, page, entryno);

		alreadyThere = true;
	}
	else
	{
		ptr->distribTimeStamp = distribTimeStamp;
		ptr->distribXid = distribXid;

		DistributedLogCtl->shared->page_dirty[slotno] = true;
	}

	LWLockRelease(DistributedLogControlLock);

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "DistributedLog_SetCommitted with local xid = %d (page = %d, entryno = %d) and distributed transaction xid = %u (timestamp = %u) status = %s",
		 localXid, page, entryno, distribXid, distribTimeStamp,
		 (alreadyThere ? "already there" : "set"));
}

/*
 * Determine if a distributed transaction committed in the distributed log.
 */
bool
DistributedLog_CommittedCheck(
	TransactionId 						localXid,
	DistributedTransactionTimeStamp		*distribTimeStamp,
	DistributedTransactionId 			*distribXid)
{
	int			page = TransactionIdToPage(localXid);
	int			entryno = TransactionIdToEntry(localXid);
	int			slotno;

	DistributedLogEntry *ptr;
	TransactionId oldestXmin;

	LWLockAcquire(DistributedLogControlLock, LW_EXCLUSIVE);

	oldestXmin = DistributedLogShared->oldestXmin;
	if (oldestXmin == InvalidTransactionId)
		elog(PANIC, "DistributedLog's OldestXmin not initialized yet");

	if (TransactionIdPrecedes(localXid, oldestXmin))
	{
		LWLockRelease(DistributedLogControlLock);

		*distribTimeStamp = 0;	// Set it to something.
		*distribXid = 0;
		return false;
	}

	slotno = SimpleLruReadPage(DistributedLogCtl, page, true, localXid);
	ptr = (DistributedLogEntry *) DistributedLogCtl->shared->page_buffer[slotno];
	ptr += entryno;
	*distribTimeStamp = ptr->distribTimeStamp;
	*distribXid = ptr->distribXid;
	ptr = NULL;
	LWLockRelease(DistributedLogControlLock);

	if (*distribTimeStamp != 0 && *distribXid != 0)
	{
		return true;
	}
	else if (*distribTimeStamp == 0 && *distribXid == 0)
	{
		// Not found.
		return false;
	}
	else
	{
		if (*distribTimeStamp == 0)
			elog(ERROR, "Found zero timestamp for local xid = %u in distributed log (distributed xid = %u, page = %d, entryno = %d)",
			     localXid, *distribXid, page, entryno);

		elog(ERROR, "Found zero distributed xid for local xid = %u in distributed log (dtx start time = %u, page = %d, entryno = %d)",
			     localXid, *distribTimeStamp, page, entryno);

		return false;	// We'll never reach here.
	}
}

/*
 * Find the next lowest transaction with a logged or recorded status.
 * Currently on distributed commits are recorded.
 */
bool
DistributedLog_ScanForPrevCommitted(
	TransactionId 						*indexXid,
	DistributedTransactionTimeStamp 	*distribTimeStamp,
	DistributedTransactionId 			*distribXid)
{
	TransactionId highXid;
	int pageno;
	TransactionId lowXid;
	int slotno;
	TransactionId xid;

	*distribTimeStamp = 0;	// Set it to something.
	*distribXid = 0;

	if ((*indexXid) == InvalidTransactionId)
		return false;
	highXid = (*indexXid) - 1;
	if (highXid < FirstNormalTransactionId)
		return false;

	while (true)
	{
		pageno = TransactionIdToPage(highXid);

		/*
		 * Compute the xid floor for the page.
		 */
		lowXid = pageno * (TransactionId) ENTRIES_PER_PAGE;
		if (lowXid == InvalidTransactionId)
			lowXid = FirstNormalTransactionId;

		LWLockAcquire(DistributedLogControlLock, LW_EXCLUSIVE);

		/*
		 * Peek to see if page exists.
		 */
		if (!SimpleLruPageExists(DistributedLogCtl, pageno))
		{
			LWLockRelease(DistributedLogControlLock);

			*indexXid = InvalidTransactionId;
			*distribTimeStamp = 0;	// Set it to something.
			*distribXid = 0;
			return false;
		}

		slotno = SimpleLruReadPage(DistributedLogCtl, pageno, true, highXid);

		for (xid = highXid; xid >= lowXid; xid--)
		{
			int						entryno = TransactionIdToEntry(xid);
			DistributedLogEntry 	*ptr;

			ptr = (DistributedLogEntry *) DistributedLogCtl->shared->page_buffer[slotno];
			ptr += entryno;

			if (ptr->distribTimeStamp != 0 && ptr->distribXid != 0)
			{
				*indexXid = xid;
				*distribTimeStamp = ptr->distribTimeStamp;
				*distribXid = ptr->distribXid;
				LWLockRelease(DistributedLogControlLock);

				return true;
			}
		}

		LWLockRelease(DistributedLogControlLock);

		if (lowXid == FirstNormalTransactionId)
		{
			*indexXid = InvalidTransactionId;
			*distribTimeStamp = 0;	// Set it to something.
			*distribXid = 0;
			return false;
		}

		highXid = lowXid - 1;	// Go to last xid of previous page.
	}

	return false;	// We'll never reach this.
}

static Size
DistributedLog_SharedShmemSize(void)
{
	return MAXALIGN(sizeof(DistributedLogShmem));
}

/*
 * Initialization of shared memory for the distributed log.
 */
Size
DistributedLog_ShmemSize(void)
{
	Size		size;

	size = SimpleLruShmemSize(NUM_DISTRIBUTEDLOG_BUFFERS, 0);

	size += DistributedLog_SharedShmemSize();

	return size;
}

void
DistributedLog_ShmemInit(void)
{
	bool		found;

	/* Set up SLRU for the distributed log. */
	DistributedLogCtl->PagePrecedes = DistributedLog_PagePrecedes;
	SimpleLruInit(DistributedLogCtl, "DistributedLogCtl", NUM_DISTRIBUTEDLOG_BUFFERS, 0,
				  DistributedLogControlLock, "pg_distributedlog");

	/* Create or attach to the shared structure */
	DistributedLogShared =
		(DistributedLogShmem *) ShmemInitStruct(
										"DistributedLogShmem",
										DistributedLog_SharedShmemSize(),
										&found);
	if (!DistributedLogShared)
		elog(FATAL, "could not initialize Distributed Log shared memory");

	if (!found)
	{
		DistributedLogShared->oldestXmin = InvalidTransactionId;
	}
}

/*
 * This func must be called ONCE on system install.  It creates
 * the initial DistributedLog segment.  (The pg_distributedlog directory is
 * assumed to have been created by the initdb shell script, and
 * DistributedLog_ShmemInit must have been called already.)
 */
void
DistributedLog_BootStrap(void)
{
	int			slotno;

	LWLockAcquire(DistributedLogControlLock, LW_EXCLUSIVE);

	/* Create and zero the first page of the commit log */
	slotno = DistributedLog_ZeroPage(0, false);

	/* Make sure it's written out */
	SimpleLruWritePage(DistributedLogCtl, slotno, NULL);
	Assert(!DistributedLogCtl->shared->page_dirty[slotno]);

	LWLockRelease(DistributedLogControlLock);
}

/*
 * Initialize (or reinitialize) a page of DistributedLog to zeroes.
 * If writeXlog is TRUE, also emit an XLOG record saying we did this.
 *
 * The page is not actually written, just set up in shared memory.
 * The slot number of the new page is returned.
 *
 * Control lock must be held at entry, and will be held at exit.
 */
static int
DistributedLog_ZeroPage(int page, bool writeXlog)
{
	int			slotno;

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "DistributedLog_ZeroPage zero page %d",
		 page);
	slotno = SimpleLruZeroPage(DistributedLogCtl, page);

	if (writeXlog)
		DistributedLog_WriteZeroPageXlogRec(page);

	return slotno;
}

/*
 * This must be called ONCE during postmaster or standalone-backend startup,
 * after StartupXLOG has initialized ShmemVariableCache->nextXid.
 */
void
DistributedLog_Startup(TransactionId oldestActiveXid,
					   TransactionId nextXid)
{
	int			startPage;
	int			endPage;

	/*
	 * UNDONE: We really need oldest frozen xid.  If we can't get it, then
	 * we will need to tolerate not finding a page in
	 * DistributedLog_SetCommitted and DistributedLog_IsCommitted.
	 */
	startPage = TransactionIdToPage(oldestActiveXid);
	endPage = TransactionIdToPage(nextXid);

	LWLockAcquire(DistributedLogControlLock, LW_EXCLUSIVE);

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "DistributedLog_Startup startPage %d, endPage %d",
		 startPage, endPage);

	/*
	 * Initialize our idea of the latest page number.
	 */
	DistributedLogCtl->shared->latest_page_number = endPage;

	/*
	 * Zero out the remainder of the current DistributedLog page.  Under normal
	 * circumstances it should be zeroes already, but it seems at least
	 * theoretically possible that XLOG replay will have settled on a nextXID
	 * value that is less than the last XID actually used and marked by the
	 * previous database lifecycle (since subtransaction commit writes clog
	 * but makes no WAL entry).  Let's just be safe. (We need not worry about
	 * pages beyond the current one, since those will be zeroed when first
	 * used.  For the same reason, there is no need to do anything when
	 * nextXid is exactly at a page boundary; and it's likely that the
	 * "current" page doesn't exist yet in that case.)
	 */
	if (TransactionIdToEntry(nextXid) != 0)
	{
		int			entryno = TransactionIdToEntry(nextXid);
		int			slotno;
		DistributedLogEntry *ptr;
		int			remainingEntries;

		slotno = SimpleLruReadPage(DistributedLogCtl, endPage, true, nextXid);
		ptr = (DistributedLogEntry *) DistributedLogCtl->shared->page_buffer[slotno];
		ptr += entryno;

		/* Zero the rest of the page */
		remainingEntries = ENTRIES_PER_PAGE - entryno;
		MemSet(ptr, 0, remainingEntries * sizeof(DistributedLogEntry));

		DistributedLogCtl->shared->page_dirty[slotno] = true;
	}

	LWLockRelease(DistributedLogControlLock);
}

/*
 * This must be called ONCE during postmaster or standalone-backend shutdown
 */
void
DistributedLog_Shutdown(void)
{
	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "DistributedLog_Shutdown");

	/* Flush dirty DistributedLog pages to disk */
	SimpleLruFlush(DistributedLogCtl, false);
}

/*
 * Perform a checkpoint --- either during shutdown, or on-the-fly
 */
void
DistributedLog_CheckPoint(void)
{
	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "DistributedLog_CheckPoint");

	/* Flush dirty DistributedLog pages to disk */
	SimpleLruFlush(DistributedLogCtl, true);
}


/*
 * Make sure that DistributedLog has room for a newly-allocated XID.
 *
 * NB: this is called while holding XidGenLock.  We want it to be very fast
 * most of the time; even when it's not so fast, no actual I/O need happen
 * unless we're forced to write out a dirty DistributedLog or xlog page
 * to make room in shared memory.
 */
void
DistributedLog_Extend(TransactionId newestXact)
{
	int			page;

	/*
	 * No work except at first XID of a page.  But beware: just after
	 * wraparound, the first XID of page zero is FirstNormalTransactionId.
	 */
	if (TransactionIdToEntry(newestXact) != 0 &&
		!TransactionIdEquals(newestXact, FirstNormalTransactionId))
		return;

	page = TransactionIdToPage(newestXact);

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "DistributedLog_Extend page %d",
		 page);

	LWLockAcquire(DistributedLogControlLock, LW_EXCLUSIVE);

	/* Zero the page and make an XLOG entry about it */
	DistributedLog_ZeroPage(page, true);

	LWLockRelease(DistributedLogControlLock);

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "DistributedLog_Extend with newest local xid = %d to page = %d",
		 newestXact, page);
}


/*
 * Remove all DistributedLog segments that are no longer needed.
 *
 * Before removing any DistributedLog data, we must flush XLOG to disk, to
 * ensure that any recently-emitted HEAP_FREEZE records have reached disk;
 * otherwise a crash and restart might leave us with some unfrozen tuples
 * referencing removed DistributedLog data.  We choose to emit a special
 * TRUNCATE XLOG record too.
 *
 * Replaying the deletion from XLOG is not critical, since the files could
 * just as well be removed later, but doing so prevents a long-running hot
 * standby server from acquiring an unreasonably bloated DistributedLog directory.
 *
 * Since DistributedLog segments hold a large number of transactions, the
 * opportunity to actually remove a segment is fairly rare, and so it seems
 * best not to do the XLOG flush unless we have confirmed that there is
 * a removable segment.
 *
 * NOTE: The caller should hold DistributedLogControlLock in exclusive mode.
 */
static void
DistributedLog_Truncate(void)
{
	int			cutoffPage;
	TransactionId oldestXmin;

	Assert(LWLockHeldByMe(DistributedLogControlLock));

	/*
	 * Get the current oldestXmin value. Anything older than that can be
	 * removed.
	 */
	oldestXmin = DistributedLogShared->oldestXmin;
	if (oldestXmin == InvalidTransactionId)
	{
		/* not initialized yet */
		return;
	}

	/*
	 * The cutoff point is the start of the segment containing oldestXact. We
	 * pass the *page* containing oldestXact to SimpleLruTruncate.
	 */
	cutoffPage = TransactionIdToPage(oldestXmin);

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "DistributedLog_Truncate with oldest local xid = %d to cutoff page = %d",
		 oldestXmin, cutoffPage);

	/* Check to see if there's any files that could be removed */
	if (!SlruScanDirectory(DistributedLogCtl, cutoffPage, false))
	{
		return;					/* nothing to remove */
	}

	/* Write XLOG record and flush XLOG to disk */
	DistributedLog_WriteTruncateXlogRec(cutoffPage);

	/* Now we can remove the old DistributedLog segment(s) */
	SimpleLruTruncateWithLock(DistributedLogCtl, cutoffPage); /* we already hold the lock */
}


/*
 * Decide which of two DistributedLog page numbers is "older" for
 * truncation purposes.
 *
 * We need to use comparison of TransactionIds here in order to do the right
 * thing with wraparound XID arithmetic.  However, if we are asked about
 * page number zero, we don't want to hand InvalidTransactionId to
 * TransactionIdPrecedes: it'll get weird about permanent xact IDs.  So,
 * offset both xids by FirstNormalTransactionId to avoid that.
 */
static bool
DistributedLog_PagePrecedes(int page1, int page2)
{
	TransactionId xid1;
	TransactionId xid2;

	xid1 = ((TransactionId) page1) * ENTRIES_PER_PAGE;
	xid1 += FirstNormalTransactionId;
	xid2 = ((TransactionId) page2) * ENTRIES_PER_PAGE;
	xid2 += FirstNormalTransactionId;

	return TransactionIdPrecedes(xid1, xid2);
}

/*
 * Write a ZEROPAGE xlog record
 *
 * Note: xlog record is marked as outside transaction control, since we
 * want it to be redone whether the invoking transaction commits or not.
 * (Besides which, this is normally done just before entering a transaction.)
 */
static void
DistributedLog_WriteZeroPageXlogRec(int page)
{
	XLogRecData rdata;

	rdata.data = (char *) (&page);
	rdata.len = sizeof(int);
	rdata.buffer = InvalidBuffer;
	rdata.next = NULL;
	(void) XLogInsert(RM_DISTRIBUTEDLOG_ID, DISTRIBUTEDLOG_ZEROPAGE, &rdata);
}

/*
 * Write a TRUNCATE xlog record
 *
 * We must flush the xlog record to disk before returning --- see notes
 * in DistributedLog_Truncate().
 *
 * Note: xlog record is marked as outside transaction control, since we
 * want it to be redone whether the invoking transaction commits or not.
 */
static void
DistributedLog_WriteTruncateXlogRec(int page)
{
	XLogRecData rdata;
	XLogRecPtr	recptr;

	rdata.data = (char *) (&page);
	rdata.len = sizeof(int);
	rdata.buffer = InvalidBuffer;
	rdata.next = NULL;
	recptr = XLogInsert(RM_DISTRIBUTEDLOG_ID, DISTRIBUTEDLOG_TRUNCATE, &rdata);
	XLogFlush(recptr);
}

/*
 * DistributedLog resource manager's routines
 */
void
DistributedLog_redo(XLogRecPtr beginLoc, XLogRecPtr lsn, XLogRecord *record)
{
	uint8		info = record->xl_info & ~XLR_INFO_MASK;

	if (info == DISTRIBUTEDLOG_ZEROPAGE)
	{
		int			page;
		int			slotno;

		memcpy(&page, XLogRecGetData(record), sizeof(int));

		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "Redo DISTRIBUTEDLOG_ZEROPAGE page %d",
			 page);

		LWLockAcquire(DistributedLogControlLock, LW_EXCLUSIVE);

		slotno = DistributedLog_ZeroPage(page, false);
		SimpleLruWritePage(DistributedLogCtl, slotno, NULL);
		Assert(!DistributedLogCtl->shared->page_dirty[slotno]);

		LWLockRelease(DistributedLogControlLock);

		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "DistributedLog_redo zero page = %d",
			 page);
	}
	else if (info == DISTRIBUTEDLOG_TRUNCATE)
	{
		int			page;

		memcpy(&page, XLogRecGetData(record), sizeof(int));

		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "Redo DISTRIBUTEDLOG_TRUNCATE page %d",
			 page);

		/*
		 * During XLOG replay, latest_page_number isn't set up yet; insert
		 * a suitable value to bypass the sanity test in SimpleLruTruncate.
		 */
		DistributedLogCtl->shared->latest_page_number = page;

		SimpleLruTruncate(DistributedLogCtl, page);

		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "DistributedLog_redo truncate to cutoff page = %d",
			 page);
	}
	else
		elog(PANIC, "DistributedLog_redo: unknown op code %u", info);
}

void
DistributedLog_desc(StringInfo buf, XLogRecPtr beginLoc, XLogRecord *record)
{
	uint8		info = record->xl_info & ~XLR_INFO_MASK;
	char		*rec = XLogRecGetData(record);

	if (info == DISTRIBUTEDLOG_ZEROPAGE)
	{
		int			page;

		memcpy(&page, rec, sizeof(int));
		appendStringInfo(buf, "zeropage: %d", page);
	}
	else if (info == DISTRIBUTEDLOG_TRUNCATE)
	{
		int			page;

		memcpy(&page, rec, sizeof(int));
		appendStringInfo(buf, "truncate before: %d", page);
	}
	else
		appendStringInfo(buf, "UNKNOWN");
}
