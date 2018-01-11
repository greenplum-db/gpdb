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

#include "miscadmin.h"

#include "access/slru.h"
#include "access/transam.h"
#include "access/distributedlog.h"
#include "utils/tqual.h"
#include "utils/guc.h"
#include "storage/shmem.h"
#include <dirent.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "cdb/cdbpersistentstore.h"

/* We need 8 bytes per xact */
#define ENTRIES_PER_PAGE (BLCKSZ / sizeof(DistributedLogEntry))

#define TransactionIdToPage(localXid) ((localXid) / (TransactionId) ENTRIES_PER_PAGE)
#define TransactionIdToEntry(localXid) ((localXid) % (TransactionId) ENTRIES_PER_PAGE)

/*
 * Link to shared-memory data structures for DistributedLog control
 */
static SlruCtlData DistributedLogCtlData;

#define DistributedLogCtl (&DistributedLogCtlData)

/*
 * Directory where the distributed logs reside within PGDATA
 */
#define DISTRIBUTEDLOG_DIR "pg_distributedlog"

typedef struct DistributedLogShmem
{
	TransactionId	oldestXid;
	bool			knowHighestUnusedPage;
	int				highestUnusedPage;	
} DistributedLogShmem;

static DistributedLogShmem *DistributedLogShared = NULL;

static void DistributedLog_SetCommitted(TransactionId localXid,
							DistributedTransactionTimeStamp dtxStartTime,
							DistributedTransactionId distribXid,
							bool isRedo);
static int	DistributedLog_ZeroPage(int page, bool writeXlog);
static bool DistributedLog_PagePrecedes(int page1, int page2);
static void DistributedLog_WriteZeroPageXlogRec(int page);
static void DistributedLog_WriteTruncateXlogRec(int page);

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

	MIRRORED_LOCK_DECLARE;

	int			page = TransactionIdToPage(localXid);
	int			entryno = TransactionIdToEntry(localXid);
	int			slotno;
	
	DistributedLogEntry *ptr;

	bool alreadyThere = false;

	MIRRORED_LOCK;
	
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

	MIRRORED_UNLOCK;
	
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
	MIRRORED_LOCK_DECLARE;

	int			page = TransactionIdToPage(localXid);
	int			entryno = TransactionIdToEntry(localXid);
	int			slotno;
	
	DistributedLogEntry *ptr;

	MIRRORED_LOCK;

	LWLockAcquire(DistributedLogControlLock, LW_EXCLUSIVE);

	if (DistributedLogShared->knowHighestUnusedPage &&
		page <= DistributedLogShared->highestUnusedPage)
	{
		/*
		 * We prevously discovered we didn't have the page...
		 */
		LWLockRelease(DistributedLogControlLock);

		MIRRORED_UNLOCK;

		*distribTimeStamp = 0;	// Set it to something.
		*distribXid = 0;

		return false;
	}
	
	/*
	 * Peek to see if page exists.
	 */
	if (!SimpleLruPageExists(DistributedLogCtl, page))
	{
		if (DistributedLogShared->knowHighestUnusedPage)
		{
			if (DistributedLogShared->highestUnusedPage > page)
				DistributedLogShared->highestUnusedPage = page;
		}
		else
		{
			DistributedLogShared->knowHighestUnusedPage = true;
			DistributedLogShared->highestUnusedPage = page;
		}
		
		LWLockRelease(DistributedLogControlLock);

		MIRRORED_UNLOCK;

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

	MIRRORED_UNLOCK;

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
	MIRRORED_LOCK_DECLARE;

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

	MIRRORED_LOCK;

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

			MIRRORED_UNLOCK;

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

				MIRRORED_UNLOCK;

				return true;
			}
		}

		LWLockRelease(DistributedLogControlLock);

		if (lowXid == FirstNormalTransactionId)
		{
			MIRRORED_UNLOCK;

			*indexXid = InvalidTransactionId;
			*distribTimeStamp = 0;	// Set it to something.
			*distribXid = 0;
			return false;
		}
		
		highXid = lowXid - 1;	// Go to last xid of previous page.
	}

	MIRRORED_UNLOCK;

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
	Size size;
	
	size = SimpleLruShmemSize(NUM_DISTRIBUTEDLOG_BUFFERS, 0);

	size += DistributedLog_SharedShmemSize();

	return size;
}

void
DistributedLog_ShmemInit(void)
{
	bool found;

	/* Set up SLRU for the distributed log. */
	DistributedLogCtl->PagePrecedes = DistributedLog_PagePrecedes;
	SimpleLruInit(DistributedLogCtl, "DistributedLogCtl", NUM_DISTRIBUTEDLOG_BUFFERS, 0,
				  DistributedLogControlLock, DISTRIBUTEDLOG_DIR);

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
		DistributedLogShared->oldestXid = InvalidTransactionId;
		DistributedLogShared->knowHighestUnusedPage = false;
		DistributedLogShared->highestUnusedPage = -1;
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
	MIRRORED_LOCK_DECLARE;

	int			slotno;

	MIRRORED_LOCK;

	LWLockAcquire(DistributedLogControlLock, LW_EXCLUSIVE);

	/* Create and zero the first page of the commit log */
	slotno = DistributedLog_ZeroPage(0, false);

	/* Make sure it's written out */
	SimpleLruWritePage(DistributedLogCtl, slotno, NULL);
	Assert(!DistributedLogCtl->shared->page_dirty[slotno]);

	LWLockRelease(DistributedLogControlLock);

	MIRRORED_UNLOCK;
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
	MIRRORED_LOCK_DECLARE;

	int			slotno;

	MIRRORED_LOCK;

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "DistributedLog_ZeroPage zero page %d", 
		 page);
	slotno = SimpleLruZeroPage(DistributedLogCtl, page);

	if (writeXlog)
		DistributedLog_WriteZeroPageXlogRec(page);

	MIRRORED_UNLOCK;

	return slotno;
}

/*
 * This must be called ONCE during postmaster or standalone-backend startup,
 * after StartupXLOG has initialized ShmemVariableCache->nextXid.
 */
void
DistributedLog_Startup(
					TransactionId oldestActiveXid,
					TransactionId nextXid)
{
	MIRRORED_LOCK_DECLARE;

	int	startPage;
	int	endPage;

	/*
	 * UNDONE: We really need oldest frozen xid.  If we can't get it, then
	 * we will need to tolerate not finding a page in
	 * DistributedLog_SetCommitted and DistributedLog_IsCommitted.
	 */
	startPage = TransactionIdToPage(oldestActiveXid);
	endPage = TransactionIdToPage(nextXid);

	MIRRORED_LOCK;

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

	MIRRORED_UNLOCK;
}

/*
 * This must be called ONCE during postmaster or standalone-backend shutdown
 */
void
DistributedLog_Shutdown(void)
{
	MIRRORED_LOCK_DECLARE;

	MIRRORED_LOCK;

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "DistributedLog_Shutdown");

	/* Flush dirty DistributedLog pages to disk */
	SimpleLruFlush(DistributedLogCtl, false);

	MIRRORED_UNLOCK;
}

/*
 * Perform a checkpoint --- either during shutdown, or on-the-fly
 */
void
DistributedLog_CheckPoint(void)
{
	MIRRORED_LOCK_DECLARE;

	MIRRORED_LOCK;

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "DistributedLog_CheckPoint");

	/* Flush dirty DistributedLog pages to disk */
	SimpleLruFlush(DistributedLogCtl, true);

	MIRRORED_UNLOCK;
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
	MIRRORED_LOCK_DECLARE;

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

	MIRRORED_LOCK;

	LWLockAcquire(DistributedLogControlLock, LW_EXCLUSIVE);

	/* Zero the page and make an XLOG entry about it */
	DistributedLog_ZeroPage(page, true);

	LWLockRelease(DistributedLogControlLock);

	MIRRORED_UNLOCK;
	
	elog((Debug_print_full_dtm ? LOG : DEBUG5), 
		 "DistributedLog_Extend with newest local xid = %d to page = %d",
		 newestXact, page);
}


/*
 * Remove all DistributedLog segments before the one holding the passed
 * transaction ID
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
 */
void
DistributedLog_Truncate(TransactionId oldestXid)
{
	MIRRORED_LOCK_DECLARE;

	int			cutoffPage;

	/*
	 * The cutoff point is the start of the segment containing oldestXact. We
	 * pass the *page* containing oldestXact to SimpleLruTruncate.
	 */
	cutoffPage = TransactionIdToPage(oldestXid);

	MIRRORED_LOCK;
	
	LWLockAcquire(DistributedLogControlLock, LW_EXCLUSIVE);
	
	/* Check to see if there's any files that could be removed */
	if (!SlruScanDirectory(DistributedLogCtl, cutoffPage, false))
	{
		LWLockRelease(DistributedLogControlLock);

		MIRRORED_UNLOCK;

		return;					/* nothing to remove */
	}

	/*
	 * Remember this as the low-water mark to aid the virtual table over the
	 * distributed log.
	 */
	DistributedLogShared->oldestXid = oldestXid;
	
	/* Write XLOG record and flush XLOG to disk */
	DistributedLog_WriteTruncateXlogRec(cutoffPage);

	/* Now we can remove the old DistributedLog segment(s) */
	SimpleLruTruncateWithLock(DistributedLogCtl, cutoffPage); /* we already hold the lock */
	
	elog((Debug_print_full_dtm ? LOG : DEBUG5), 
		 "DistributedLog_Truncate with oldest local xid = %d to cutoff page = %d",
		 oldestXid, cutoffPage);
	
	LWLockRelease(DistributedLogControlLock);

	MIRRORED_UNLOCK;
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
	MIRRORED_LOCK_DECLARE;

	uint8		info = record->xl_info & ~XLR_INFO_MASK;

	MIRRORED_LOCK;

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

	MIRRORED_UNLOCK;
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

