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
#include "cdb/cdbtm.h"
#include "cdb/cdbvars.h"
#include "port/atomics.h"
#include "storage/shmem.h"
#include "utils/faultinjector.h"
#include "utils/guc.h"
#include "miscadmin.h"
#include "libpq/libpq-be.h" /* struct Port */

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
#define TransactionIdToSegment(localXid) ((localXid) / (ENTRIES_PER_PAGE * SLRU_PAGES_PER_SEGMENT))

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
	volatile TransactionId	oldestXmin;

} DistributedLogShmem;

static DistributedLogShmem *DistributedLogShared = NULL;

static int	DistributedLog_ZeroPage(int page, bool writeXlog);
static bool DistributedLog_PagePrecedes(int page1, int page2);
static void DistributedLog_WriteZeroPageXlogRec(int page);
static void DistributedLog_WriteTruncateXlogRec(int page);
static void DistributedLog_Truncate(TransactionId oldestXmin);

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
void
DistributedLog_InitOldestXmin(void)
{
	TransactionId oldestXmin = ShmemVariableCache->oldestXid;
	TransactionId latestXid = ShmemVariableCache->latestCompletedXid;

	/*
	 * Start scanning from oldest datfrozenxid, until we find a
	 * valid page.
	 */
	for (;;)
	{
		int			page = TransactionIdToPage(oldestXmin);
		TransactionId xid;

		if (SimpleLruDoesPhysicalPageExist(DistributedLogCtl, page))
		{
			/* Found the beginning of valid distributedlog */
			break;
		}

		/* Advance to the first XID on the next page */
		xid = AdvanceTransactionIdToNextPage(oldestXmin);

		/* but don't go past oldestLocalXmin */
		if (TransactionIdFollows(xid, latestXid))
		{
			oldestXmin = latestXid;
			break;
		}

		oldestXmin = xid;
	}

	pg_atomic_write_u32((pg_atomic_uint32 *)&DistributedLogShared->oldestXmin, oldestXmin);
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
	TransactionId oldOldestXmin;
	int			currPage;
	int			slotno;
	bool		DistributedLogControlLockHeldByMe = false;
	DistributedLogEntry *entries = NULL;

	Assert(!IS_QUERY_DISPATCHER());
	Assert(TransactionIdIsNormal(oldestLocalXmin));

#ifdef FAULT_INJECTOR
	const char *dbname = NULL;
	if (MyProcPort)
		dbname = MyProcPort->database_name;

	FaultInjector_InjectFaultIfSet("distributedlog_advance_oldest_xmin", DDLNotSpecified,
								   dbname?dbname: "", "");
#endif

	LWLockAcquire(DistributedLogTruncateLock, LW_SHARED);
	oldestXmin = (TransactionId)pg_atomic_read_u32((pg_atomic_uint32 *)&DistributedLogShared->oldestXmin);
	oldOldestXmin = oldestXmin;
	Assert(oldestXmin != InvalidTransactionId);

	/*
	 * oldestXmin (DistributedLogShared->oldestXmin) can be higher than
	 * oldestLocalXmin (globalXmin in GetSnapshotData()) in concurrent
	 * work-load. This happens due to fact that GetSnapshotData() loops over
	 * procArray and releases the ProcArrayLock before reaching here. So, if
	 * oldestXmin has already bumped ahead of oldestLocalXmin its safe to just
	 * return oldestXmin, as some other process already checked the
	 * distributed log for us.
	 */
	currPage = -1;
	while (TransactionIdPrecedes(oldestXmin, oldestLocalXmin))
	{
		int			page = TransactionIdToPage(oldestXmin);
		int			entryno = TransactionIdToEntry(oldestXmin);
		DistributedLogEntry *ptr;

		if (page != currPage)
		{
			/*
			 * SimpleLruReadPage_ReadOnly will acquire a lwlock, it is the
			 * caller's responsibility to release this lock.
			 * But we cannot release the lock immediately in one run of the
			 * loop, since the entries of current buffer will still be used
			 * by other items in the same page.
			 * So we release the lock when encountering a new page.
			 */
			if (DistributedLogControlLockHeldByMe)
			{
				Assert(LWLockHeldByMe(DistributedLogControlLock));
				LWLockRelease(DistributedLogControlLock);
			}
			slotno = SimpleLruReadPage_ReadOnly(DistributedLogCtl, page, oldestXmin);
			DistributedLogControlLockHeldByMe = true;
			currPage = page;
			/* entries is protected by the DistributedLogControl shared lock */
			entries = (DistributedLogEntry *) DistributedLogCtl->shared->page_buffer[slotno];
		}

		ptr = &entries[entryno];
		/*
		 * If this XID is already visible to all distributed snapshots, we can
		 * advance past it. Otherwise stop here. (Local-only transactions will
		 * have zeros in distribXid and distribTimeStamp; this test will also
		 * skip over those.)
		 *
		 * And the distributed xid is just a plain counter, so we just use the `>=` for
		 * the comparison of gxid
		 */
		if (ptr->distribTimeStamp == distribTransactionTimeStamp &&
				ptr->distribXid >= xminAllDistributedSnapshots)
			break;

		TransactionIdAdvance(oldestXmin);
	}
	if (DistributedLogControlLockHeldByMe)
	{
		Assert(LWLockHeldByMe(DistributedLogControlLock));
		LWLockRelease(DistributedLogControlLock);
	}

	/*
	 * The shared oldestXmin (DistributedLogShared->oldestXmin) may be updated
	 * concurrently. It should be set to a higher value, because a higher xmin
	 * can belong to another distributed log segment, its older segments might
	 * already be truncated.
	 */
	if (!TransactionIdEquals(oldOldestXmin, oldestXmin))
	{
		uint32 expected = (uint32)oldOldestXmin;

		while (1)
		{
			if (pg_atomic_compare_exchange_u32((pg_atomic_uint32 *)&DistributedLogShared->oldestXmin, 
											&expected, (uint32)oldestXmin))
				break;

			if (TransactionIdPrecedesOrEquals(oldestXmin, expected))
				break;
		}
	}

	LWLockRelease(DistributedLogTruncateLock);

	if (TransactionIdToSegment(oldOldestXmin) < TransactionIdToSegment(oldestXmin))
		DistributedLog_Truncate(oldestXmin);

	return oldestXmin;
}

/*
 * Return the "oldest xmin" among distributed snapshots.
 */
TransactionId
DistributedLog_GetOldestXmin(TransactionId oldestLocalXmin)
{
	TransactionId result;

	result = (TransactionId)pg_atomic_read_u32((pg_atomic_uint32 *)&DistributedLogShared->oldestXmin);

	Assert(!IS_QUERY_DISPATCHER());
	elogif(Debug_print_full_dtm, LOG, "distributed oldestXmin is '%u'", result);

	/*
	 * Like in DistributedLog_AdvanceOldestXmin(), the shared oldestXmin
	 * might already have been advanced past oldestLocalXmin.
	 */
	if (!TransactionIdIsValid(result) ||
		TransactionIdFollows(result, oldestLocalXmin))
		result = oldestLocalXmin;

	return result;
}

/*
 * Record that a distributed transaction committed in the distributed log for
 * all transaction ids on a single page. This function is similar to clog
 * function TransactionIdSetTreeStatus().
 */
static void
DistributedLog_SetCommittedWithinAPage(
	int                                 numLocIds,
	TransactionId 						*localXid,
	DistributedTransactionTimeStamp		distribTimeStamp,
	DistributedTransactionId 			distribXid,
	bool								isRedo)
{
	int page;
	int slotno;
	DistributedLogEntry *ptr;

	Assert(!IS_QUERY_DISPATCHER());
	Assert(numLocIds > 0);
	Assert(localXid != NULL);
	Assert(TransactionIdIsValid(localXid[0]));

	page = TransactionIdToPage(localXid[0]);

	LWLockAcquire(DistributedLogControlLock, LW_EXCLUSIVE);

	if (isRedo)
	{
		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "DistributedLog_SetCommitted check if page %d is present",
			 page);
		if (!SimpleLruDoesPhysicalPageExist(DistributedLogCtl, page))
		{
			DistributedLog_ZeroPage(page, /* writeXLog */ false);
			elog((Debug_print_full_dtm ? LOG : DEBUG5),
				 "DistributedLog_SetCommitted zeroed page %d",
				 page);
		}
	}

	slotno = SimpleLruReadPage(DistributedLogCtl, page, true, localXid[0]);
	ptr = (DistributedLogEntry *) DistributedLogCtl->shared->page_buffer[slotno];

	for (int i = 0; i < numLocIds; i++)
	{
		bool alreadyThere = false;
		Assert(TransactionIdToPage(localXid[i]) == page);
		Assert(TransactionIdIsValid(localXid[i]));

		int	entryno = TransactionIdToEntry(localXid[i]);

		if (ptr[entryno].distribTimeStamp != 0 || ptr[entryno].distribXid != 0)
		{
			if (ptr[entryno].distribTimeStamp != distribTimeStamp)
				elog(ERROR,
					 "Current distributed timestamp = %u does not match input timestamp = %u for local xid = %u in distributed log (page = %d, entryno = %d)",
					 ptr[entryno].distribTimeStamp, distribTimeStamp,
					 localXid[i], page, entryno);

			if (ptr[entryno].distribXid != distribXid)
				elog(ERROR,
					 "Current distributed xid = %u does not match input distributed xid = %u for local xid = %u in distributed log (page = %d, entryno = %d)",
					 ptr[entryno].distribXid, distribXid, localXid[i], page, entryno);

			alreadyThere = true;
		}
		else
		{
			ptr[entryno].distribTimeStamp = distribTimeStamp;
			ptr[entryno].distribXid = distribXid;

			DistributedLogCtl->shared->page_dirty[slotno] = true;
		}

		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "DistributedLog_SetCommitted with local xid = %d (page = %d, entryno = %d) and distributed transaction xid = %u (timestamp = %u) status = %s",
			 localXid[i], page, entryno, distribXid, distribTimeStamp,
			 (alreadyThere ? "already there" : "set"));
	}

	LWLockRelease(DistributedLogControlLock);
}

/*
 * Set committed for a bunch of transactions, chunking in the separate DLOG
 * pages involved. This function is similar to clog function
 * TransactionIdSetTreeStatus().
 */
static void
DistributedLog_SetCommittedByPages(int nsubxids, TransactionId *subxids,
								   DistributedTransactionTimeStamp distribTimeStamp,
								   DistributedTransactionId distribXid,
								   bool isRedo)
{
	int			i = 0;

	while (i < nsubxids)
	{
		int	num_on_page = 0;
		/* This points in subxids array the start of transaction ids on a given page */
		int start_of_range = i;
		int pageno = TransactionIdToPage(subxids[start_of_range]);

		while (TransactionIdToPage(subxids[i]) == pageno && i < nsubxids)
		{
			num_on_page++;
			i++;
		}

		DistributedLog_SetCommittedWithinAPage(num_on_page, subxids + start_of_range,
											   distribTimeStamp, distribXid, isRedo);
	}
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
	if (!IS_QUERY_DISPATCHER())
	{
		DistributedLog_SetCommittedWithinAPage(1, &xid, distribTimeStamp,
											   distribXid, isRedo);

		/* add entry for sub-transaction page at time */
		DistributedLog_SetCommittedByPages(nxids, xids, distribTimeStamp,
										   distribXid, isRedo);
	}
}

/*
 * Get the corresponding distributed xid and timestamp of a local xid.
 */
void
DistributedLog_GetDistributedXid(
	TransactionId 						localXid,
	DistributedTransactionTimeStamp		*distribTimeStamp,
	DistributedTransactionId 			*distribXid)
{
	int			page = TransactionIdToPage(localXid);
	int			entryno = TransactionIdToEntry(localXid);
	int			slotno;
	DistributedLogEntry *ptr;

	Assert(!IS_QUERY_DISPATCHER());

	slotno = SimpleLruReadPage_ReadOnly(DistributedLogCtl, page, localXid);
	ptr = (DistributedLogEntry *) DistributedLogCtl->shared->page_buffer[slotno];
	ptr += entryno;
	*distribTimeStamp = ptr->distribTimeStamp;
	*distribXid = ptr->distribXid;
	LWLockRelease(DistributedLogControlLock);
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

	Assert(!IS_QUERY_DISPATCHER());

	DistributedLogEntry *ptr;
	TransactionId oldestXmin;


	oldestXmin = (TransactionId)pg_atomic_read_u32((pg_atomic_uint32 *)&DistributedLogShared->oldestXmin);
	if (oldestXmin == InvalidTransactionId)
		elog(PANIC, "DistributedLog's OldestXmin not initialized yet");

	if (TransactionIdPrecedes(localXid, oldestXmin))
	{
		*distribTimeStamp = 0;	// Set it to something.
		*distribXid = 0;
		return false;
	}

	LWLockAcquire(DistributedLogTruncateLock, LW_SHARED);
	LWLockAcquire(DistributedLogControlLock, LW_EXCLUSIVE);

	slotno = SimpleLruReadPage(DistributedLogCtl, page, true, localXid);
	ptr = (DistributedLogEntry *) DistributedLogCtl->shared->page_buffer[slotno];
	ptr += entryno;
	*distribTimeStamp = ptr->distribTimeStamp;
	*distribXid = ptr->distribXid;
	ptr = NULL;
	LWLockRelease(DistributedLogControlLock);
	LWLockRelease(DistributedLogTruncateLock);

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
		if (!SimpleLruDoesPhysicalPageExist(DistributedLogCtl, pageno))
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

	if (IS_QUERY_DISPATCHER())
	{
		size = 0;
	}
	else
	{
		size = SimpleLruShmemSize(NUM_DISTRIBUTEDLOG_BUFFERS, 0);
		size += DistributedLog_SharedShmemSize();
	}

	return size;
}

void
DistributedLog_ShmemInit(void)
{
	bool		found;

	if (IS_QUERY_DISPATCHER())
		return;

	/* Set up SLRU for the distributed log. */
	DistributedLogCtl->PagePrecedes = DistributedLog_PagePrecedes;
	SimpleLruInit(DistributedLogCtl, "DistributedLogCtl", NUM_DISTRIBUTEDLOG_BUFFERS, 0,
				  DistributedLogControlLock, "pg_distributedlog",
				  LWTRANCHE_DISTRIBUTEDLOG_BUFFERS);

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

	if (IS_QUERY_DISPATCHER())
		return;

	LWLockAcquire(DistributedLogControlLock, LW_EXCLUSIVE);

	/* Create and zero the first page of the commit log */
	slotno = DistributedLog_ZeroPage(0, false);

	/* Make sure it's written out */
	SimpleLruWritePage(DistributedLogCtl, slotno);
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

	Assert(!IS_QUERY_DISPATCHER());

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

	if (IS_QUERY_DISPATCHER())
		return;

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
	 * In situations where new segments' data directories are copied from the
	 * master (such as binary upgrade), the distributed logs inherited by the
	 * segment will be incomplete. This is because master doesn't maintain these
	 * logs past their initial creation. In these cases (and these cases only!),
	 * we need to initialize and zero out log pages in memory for the range of
	 * active XIDs.
	 *
	 * TODO: Turn off distributed logging during binary upgrade to avoid the
	 * issue mentioned above.
	 */
	if (IsBinaryUpgrade || ConvertMasterDataDirToSegment)
	{
		int currentPage = startPage;

		/*
		 * The below loop has a defined exit condition as long as our pages are
		 * within a sane range.
		 */
		Assert(currentPage <= TransactionIdToPage(MaxTransactionId));
		Assert(endPage <= TransactionIdToPage(MaxTransactionId));

		/*
		 * Clean the pg_distributedlog directory
		 */
		SimpleLruTruncateWithLock(DistributedLogCtl, currentPage);

		do
		{
			if (currentPage > TransactionIdToPage(MaxTransactionId))
				currentPage = 0;

			DistributedLog_ZeroPage(currentPage, false);
		}
		while (currentPage++ != endPage);
	}

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

	DistributedLog_InitOldestXmin();

	LWLockRelease(DistributedLogControlLock);
}

/*
 * This must be called ONCE during postmaster or standalone-backend shutdown
 */
void
DistributedLog_Shutdown(void)
{
	if (IS_QUERY_DISPATCHER())
		return;

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
	if (IS_QUERY_DISPATCHER())
		return;

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

	if (IS_QUERY_DISPATCHER())
		return;

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
 * DistributedLog is consulted for transactions that are committed but appear
 * as in-progress to a snapshot.  Segments that hold status of transactions
 * older than the oldest xmin of all distributed snapshots are no longer
 * needed.
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
 */
static void
DistributedLog_Truncate(TransactionId oldestXmin)
{
	int			cutoffPage;

	Assert(!IS_QUERY_DISPATCHER());

	LWLockAcquire(DistributedLogTruncateLock, LW_EXCLUSIVE);
	/*
	 * The cutoff point is the start of the segment containing oldestXact. We
	 * pass the *page* containing oldestXact to SimpleLruTruncate.
	 */
	cutoffPage = TransactionIdToPage(oldestXmin);

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "DistributedLog_Truncate with oldest local xid = %d to cutoff page = %d",
		 oldestXmin, cutoffPage);

	/* Check to see if there's any files that could be removed */
	if (!SlruScanDirectory(DistributedLogCtl, SlruScanDirCbReportPresence, &cutoffPage))
	{
		LWLockRelease(DistributedLogTruncateLock);
		return;					/* nothing to remove */
	}

	/* Write XLOG record and flush XLOG to disk */
	DistributedLog_WriteTruncateXlogRec(cutoffPage);

	/* Now we can remove the old DistributedLog segment(s) */
	SimpleLruTruncate(DistributedLogCtl, cutoffPage);
	LWLockRelease(DistributedLogTruncateLock);
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
	XLogBeginInsert();
	XLogRegisterData((char *) (&page), sizeof(int));
	(void) XLogInsert(RM_DISTRIBUTEDLOG_ID, DISTRIBUTEDLOG_ZEROPAGE);
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
	XLogRecPtr	recptr;

	XLogBeginInsert();
	XLogRegisterData((char *) (&page), sizeof(int));
	recptr = XLogInsert(RM_DISTRIBUTEDLOG_ID, DISTRIBUTEDLOG_TRUNCATE);
	XLogFlush(recptr);
}

/*
 * DistributedLog resource manager's routines
 */
void
DistributedLog_redo(XLogReaderState *record)
{
	uint8		info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;
	Assert(!IS_QUERY_DISPATCHER());

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
		SimpleLruWritePage(DistributedLogCtl, slotno);
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
