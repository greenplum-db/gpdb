/*-------------------------------------------------------------------------
 *
 * slru.c
 *		Simple LRU buffering for transaction status logfiles
 *
 * We use a simple least-recently-used scheme to manage a pool of page
 * buffers.  Under ordinary circumstances we expect that write
 * traffic will occur mostly to the latest page (and to the just-prior
 * page, soon after a page transition).  Read traffic will probably touch
 * a larger span of pages, but in any case a fairly small number of page
 * buffers should be sufficient.  So, we just search the buffers using plain
 * linear search; there's no need for a hashtable or anything fancy.
 * The management algorithm is straight LRU except that we will never swap
 * out the latest page (since we know it's going to be hit again eventually).
 *
 * We use a control LWLock to protect the shared data structures, plus
 * per-buffer LWLocks that synchronize I/O for each buffer.  The control lock
 * must be held to examine or modify any shared state.	A process that is
 * reading in or writing out a page buffer does not hold the control lock,
 * only the per-buffer lock for the buffer it is working on.
 *
 * "Holding the control lock" means exclusive lock in all cases except for
 * SimpleLruReadPage_ReadOnly(); see comments for SlruRecentlyUsed() for
 * the implications of that.
 *
 * When initiating I/O on a buffer, we acquire the per-buffer lock exclusively
 * before releasing the control lock.  The per-buffer lock is released after
 * completing the I/O, re-acquiring the control lock, and updating the shared
 * state.  (Deadlock is not possible here, because we never try to initiate
 * I/O when someone else is already doing I/O on the same buffer.)
 * To wait for I/O to complete, release the control lock, acquire the
 * per-buffer lock in shared mode, immediately release the per-buffer lock,
 * reacquire the control lock, and then recheck state (since arbitrary things
 * could have happened while we didn't have the lock).
 *
 * As with the regular buffer manager, it is possible for another process
 * to re-dirty a page that is currently being written out.	This is handled
 * by re-setting the page's page_dirty flag.
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/backend/access/transam/slru.c,v 1.47 2009/04/02 20:59:10 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "access/slru.h"
#include "access/transam.h"
#include "access/xlog.h"
#include "storage/fd.h"
#include "storage/shmem.h"
#include "miscadmin.h"

#include "cdb/cdbfilerepprimary.h"
#include "cdb/cdbmirroredflatfile.h"
#include "postmaster/primary_mirror_mode.h"
#include "libpq/md5.h"


/*
 * Define segment size.  A page is the same BLCKSZ as is used everywhere
 * else in Postgres.  The segment size can be chosen somewhat arbitrarily;
 * we make it 32 pages by default, or 256Kb, i.e. 1M transactions for CLOG
 * or 64K transactions for SUBTRANS.
 *
 * Note: because TransactionIds are 32 bits and wrap around at 0xFFFFFFFF,
 * page numbering also wraps around at 0xFFFFFFFF/xxxx_XACTS_PER_PAGE (where
 * xxxx is CLOG or SUBTRANS, respectively), and segment numbering at
 * 0xFFFFFFFF/xxxx_XACTS_PER_PAGE/SLRU_PAGES_PER_SEGMENT.  We need
 * take no explicit notice of that fact in this module, except when comparing
 * segment and page numbers in SimpleLruTruncate (see PagePrecedes()).
 *
 * Note: this file currently assumes that segment file names will be four
 * hex digits.	This sets a lower bound on the segment size (64K transactions
 * for 32-bit TransactionIds).
 */
#define SLRU_PAGES_PER_SEGMENT	32

#define SlruFileName(ctl, path, seg) \
	snprintf(path, MAXPGPATH, "%s/%04X", (ctl)->Dir, seg)
#define SlruSimpleFileName(path, seg) \
		snprintf(path, MAXPGPATH, "%04X", seg)

/*
 * During SimpleLruFlush(), we will usually not need to write/fsync more
 * than one or two physical files, but we may need to write several pages
 * per file.  We can consolidate the I/O requests by leaving files open
 * until control returns to SimpleLruFlush().  This data structure remembers
 * which files are open.
 */
#define MAX_FLUSH_BUFFERS	16

typedef struct SlruFlushData
{
	int			num_files;		/* # files actually open */

	MirroredFlatFileOpen	mirroredOpens[MAX_FLUSH_BUFFERS];

	int			segno[MAX_FLUSH_BUFFERS];		/* their log seg#s */
} SlruFlushData;

/*
 * Macro to mark a buffer slot "most recently used".  Note multiple evaluation
 * of arguments!
 *
 * The reason for the if-test is that there are often many consecutive
 * accesses to the same page (particularly the latest page).  By suppressing
 * useless increments of cur_lru_count, we reduce the probability that old
 * pages' counts will "wrap around" and make them appear recently used.
 *
 * We allow this code to be executed concurrently by multiple processes within
 * SimpleLruReadPage_ReadOnly().  As long as int reads and writes are atomic,
 * this should not cause any completely-bogus values to enter the computation.
 * However, it is possible for either cur_lru_count or individual
 * page_lru_count entries to be "reset" to lower values than they should have,
 * in case a process is delayed while it executes this macro.  With care in
 * SlruSelectLRUPage(), this does little harm, and in any case the absolute
 * worst possible consequence is a nonoptimal choice of page to evict.	The
 * gain from allowing concurrent reads of SLRU pages seems worth it.
 */
#define SlruRecentlyUsed(shared, slotno)	\
	do { \
		int		new_lru_count = (shared)->cur_lru_count; \
		if (new_lru_count != (shared)->page_lru_count[slotno]) { \
			(shared)->cur_lru_count = ++new_lru_count; \
			(shared)->page_lru_count[slotno] = new_lru_count; \
		} \
	} while (0)

/* Saved info for SlruReportIOError */
typedef enum
{
	SLRU_OPEN_FAILED,
	SLRU_SEEK_FAILED,
	SLRU_READ_FAILED,
	SLRU_WRITE_FAILED,
	SLRU_FSYNC_FAILED,
	SLRU_CLOSE_FAILED
} SlruErrorCause;

static SlruErrorCause slru_errcause;
static int	slru_errno;


/*
 * GUC variable to control the batch size used to display the
 * total number of files that get shipped to the mirror.
 * For e.g After every 1000 files have been shipped to the mirror,
 * a log message is printed indicating the total number of
 * files shipped to the mirror.
 */
int log_count_recovered_files_batch = 1000;

static int SimpleLruReadPage_Internal(SlruCtl ctl, int pageno, bool write_ok, TransactionId xid, bool *valid);
static void SimpleLruZeroLSNs(SlruCtl ctl, int slotno);
static void SimpleLruWaitIO(SlruCtl ctl, int slotno);
static bool SlruPhysicalReadPage(SlruCtl ctl, int pageno, int slotno);
static bool SlruPhysicalWritePage(SlruCtl ctl, int pageno, int slotno,
					  SlruFlush fdata);
static void SlruReportIOError(SlruCtl ctl, int pageno, TransactionId xid);
static int	SlruSelectLRUPage(SlruCtl ctl, int pageno);
static int SlruRecoverMirrorDir(char *dirName);
static int SlruVerifyDirectoryChecksum(char *fullDirName);
static bool isSlruFileName(const char *fileName);
static int SlruComputeChecksum(char *filePath, char *md5);
static int SlruCopyDirectory(char *dirName, char *fullDirName);


/*
 * Initialization of shared memory
 */

Size
SimpleLruShmemSize(int nslots, int nlsns)
{
	Size		sz;

	/* we assume nslots isn't so large as to risk overflow */
	sz = MAXALIGN(sizeof(SlruSharedData));
	sz += MAXALIGN(nslots * sizeof(char *));	/* page_buffer[] */
	sz += MAXALIGN(nslots * sizeof(SlruPageStatus));	/* page_status[] */
	sz += MAXALIGN(nslots * sizeof(bool));		/* page_dirty[] */
	sz += MAXALIGN(nslots * sizeof(int));		/* page_number[] */
	sz += MAXALIGN(nslots * sizeof(int));		/* page_lru_count[] */
	sz += MAXALIGN(nslots * sizeof(LWLockId));	/* buffer_locks[] */

	if (nlsns > 0)
		sz += MAXALIGN(nslots * nlsns * sizeof(XLogRecPtr));	/* group_lsn[] */

	return BUFFERALIGN(sz) + BLCKSZ * nslots;
}

void
SimpleLruInit(SlruCtl ctl, const char *name, int nslots, int nlsns,
			  LWLockId ctllock, const char *subdir)
{
	SlruShared	shared;
	bool		found;

	shared = (SlruShared) ShmemInitStruct(name,
										  SimpleLruShmemSize(nslots, nlsns),
										  &found);

	if (!IsUnderPostmaster)
	{
		/* Initialize locks and shared memory area */
		char	   *ptr;
		Size		offset;
		int			slotno;

		Assert(!found);

		memset(shared, 0, sizeof(SlruSharedData));

		shared->ControlLock = ctllock;

		shared->num_slots = nslots;
		shared->lsn_groups_per_page = nlsns;

		shared->cur_lru_count = 0;

		/* shared->latest_page_number will be set later */

		ptr = (char *) shared;
		offset = MAXALIGN(sizeof(SlruSharedData));
		shared->page_buffer = (char **) (ptr + offset);
		offset += MAXALIGN(nslots * sizeof(char *));
		shared->page_status = (SlruPageStatus *) (ptr + offset);
		offset += MAXALIGN(nslots * sizeof(SlruPageStatus));
		shared->page_dirty = (bool *) (ptr + offset);
		offset += MAXALIGN(nslots * sizeof(bool));
		shared->page_number = (int *) (ptr + offset);
		offset += MAXALIGN(nslots * sizeof(int));
		shared->page_lru_count = (int *) (ptr + offset);
		offset += MAXALIGN(nslots * sizeof(int));
		shared->buffer_locks = (LWLockId *) (ptr + offset);
		offset += MAXALIGN(nslots * sizeof(LWLockId));

		if (nlsns > 0)
		{
			shared->group_lsn = (XLogRecPtr *) (ptr + offset);
			offset += MAXALIGN(nslots * nlsns * sizeof(XLogRecPtr));
		}

		ptr += BUFFERALIGN(offset);
		for (slotno = 0; slotno < nslots; slotno++)
		{
			shared->page_buffer[slotno] = ptr;
			shared->page_status[slotno] = SLRU_PAGE_EMPTY;
			shared->page_dirty[slotno] = false;
			shared->page_lru_count[slotno] = 0;
			shared->buffer_locks[slotno] = LWLockAssign();
			ptr += BLCKSZ;
		}
	}
	else
	{
		Assert(found);
	}

	/*
	 * Initialize the unshared control struct, including directory path. We
	 * assume caller set PagePrecedes.
	 */
	ctl->shared = shared;
	ctl->do_fsync = true;		/* default behavior */
	StrNCpy(ctl->Dir, subdir, sizeof(ctl->Dir));
}

/*
 * Initialize (or reinitialize) a page to zeroes.
 *
 * The page is not actually written, just set up in shared memory.
 * The slot number of the new page is returned.
 *
 * Control lock must be held at entry, and will be held at exit.
 */
int
SimpleLruZeroPage(SlruCtl ctl, int pageno)
{
	SlruShared	shared = ctl->shared;
	int			slotno;

	/* Find a suitable buffer slot for the page */
	slotno = SlruSelectLRUPage(ctl, pageno);
	Assert(shared->page_status[slotno] == SLRU_PAGE_EMPTY ||
		   (shared->page_status[slotno] == SLRU_PAGE_VALID &&
			!shared->page_dirty[slotno]) ||
		   shared->page_number[slotno] == pageno);

	/* Mark the slot as containing this page */
	shared->page_number[slotno] = pageno;
	shared->page_status[slotno] = SLRU_PAGE_VALID;
	shared->page_dirty[slotno] = true;
	SlruRecentlyUsed(shared, slotno);

	/* Set the buffer to zeroes */
	MemSet(shared->page_buffer[slotno], 0, BLCKSZ);

	/* Set the LSNs for this new page to zero */
	SimpleLruZeroLSNs(ctl, slotno);

	/* Assume this page is now the latest active page */
	shared->latest_page_number = pageno;

	return slotno;
}

/*
 * Zero all the LSNs we store for this slru page.
 *
 * This should be called each time we create a new page, and each time we read
 * in a page from disk into an existing buffer.  (Such an old page cannot
 * have any interesting LSNs, since we'd have flushed them before writing
 * the page in the first place.)
 */
static void
SimpleLruZeroLSNs(SlruCtl ctl, int slotno)
{
	SlruShared	shared = ctl->shared;

	if (shared->lsn_groups_per_page > 0)
		MemSet(&shared->group_lsn[slotno * shared->lsn_groups_per_page], 0,
			   shared->lsn_groups_per_page * sizeof(XLogRecPtr));
}

/*
 * Wait for any active I/O on a page slot to finish.  (This does not
 * guarantee that new I/O hasn't been started before we return, though.
 * In fact the slot might not even contain the same page anymore.)
 *
 * Control lock must be held at entry, and will be held at exit.
 */
static void
SimpleLruWaitIO(SlruCtl ctl, int slotno)
{
	SlruShared	shared = ctl->shared;

	/* See notes at top of file */
	LWLockRelease(shared->ControlLock);
	LWLockAcquire(shared->buffer_locks[slotno], LW_SHARED);
	LWLockRelease(shared->buffer_locks[slotno]);
	LWLockAcquire(shared->ControlLock, LW_EXCLUSIVE);

	/*
	 * If the slot is still in an io-in-progress state, then either someone
	 * already started a new I/O on the slot, or a previous I/O failed and
	 * neglected to reset the page state.  That shouldn't happen, really, but
	 * it seems worth a few extra cycles to check and recover from it. We can
	 * cheaply test for failure by seeing if the buffer lock is still held (we
	 * assume that transaction abort would release the lock).
	 */
	if (shared->page_status[slotno] == SLRU_PAGE_READ_IN_PROGRESS ||
		shared->page_status[slotno] == SLRU_PAGE_WRITE_IN_PROGRESS)
	{
		if (LWLockConditionalAcquire(shared->buffer_locks[slotno], LW_SHARED))
		{
			/* indeed, the I/O must have failed */
			if (shared->page_status[slotno] == SLRU_PAGE_READ_IN_PROGRESS)
				shared->page_status[slotno] = SLRU_PAGE_EMPTY;
			else	/* write_in_progress */
			{
				shared->page_status[slotno] = SLRU_PAGE_VALID;
				shared->page_dirty[slotno] = true;
			}
			LWLockRelease(shared->buffer_locks[slotno]);
		}
	}
}


int
SimpleLruReadPage(SlruCtl ctl, int pageno, bool write_ok, TransactionId xid)
{
  return SimpleLruReadPage_Internal(ctl, pageno, write_ok, xid, NULL);
}  /* end SimpleLruReadPage */


/*
 * Find a page in a shared buffer, reading it in if necessary.
 * The page number must correspond to an already-initialized page.
 *
 * If write_ok is true then it is OK to return a page that is in
 * WRITE_IN_PROGRESS state; it is the caller's responsibility to be sure
 * that modification of the page is safe.  If write_ok is false then we
 * will not return the page until it is not undergoing active I/O.
 *
 * The passed-in xid is used only for error reporting, and may be
 * InvalidTransactionId if no specific xid is associated with the action.
 *
 * If the passed in pointer to valid is NULL, then log errors can be
 * generated by this function. If valid is not NULL, then the function 
 * will not generate log errors, but will set the boolean value
 * pointed to by valid to TRUE if it was able to read the page,    
 * or FALSE if the page read had error.
 *                   
 * Return value is the shared-buffer slot number now holding the page.
 * The buffer's LRU access info is updated.
 *
 * Control lock must be held at entry, and will be held at exit.
 */
static int
SimpleLruReadPage_Internal(SlruCtl ctl, int pageno, bool write_ok, TransactionId xid, bool *valid)
{
	SlruShared	shared = ctl->shared;

	/* Outer loop handles restart if we must wait for someone else's I/O */
	for (;;)
	{
		int			slotno;
		bool		ok;

		/* See if page already is in memory; if not, pick victim slot */
		slotno = SlruSelectLRUPage(ctl, pageno);

		/* Did we find the page in memory? */
		if (shared->page_number[slotno] == pageno &&
			shared->page_status[slotno] != SLRU_PAGE_EMPTY)
		{
			/*
			 * If page is still being read in, we must wait for I/O.  Likewise
			 * if the page is being written and the caller said that's not OK.
			 */
			if (shared->page_status[slotno] == SLRU_PAGE_READ_IN_PROGRESS ||
				(shared->page_status[slotno] == SLRU_PAGE_WRITE_IN_PROGRESS &&
				 !write_ok))
			{
				SimpleLruWaitIO(ctl, slotno);
				/* Now we must recheck state from the top */
				continue;
			}
			/* Otherwise, it's ready to use */
			SlruRecentlyUsed(shared, slotno);
			if (valid != NULL)
			   *valid = true;
			return slotno;
		}

		/* We found no match; assert we selected a freeable slot */
		Assert(shared->page_status[slotno] == SLRU_PAGE_EMPTY ||
			   (shared->page_status[slotno] == SLRU_PAGE_VALID &&
				!shared->page_dirty[slotno]));

		/* Mark the slot read-busy */
		shared->page_number[slotno] = pageno;
		shared->page_status[slotno] = SLRU_PAGE_READ_IN_PROGRESS;
		shared->page_dirty[slotno] = false;

		/* Acquire per-buffer lock (cannot deadlock, see notes at top) */
		LWLockAcquire(shared->buffer_locks[slotno], LW_EXCLUSIVE);

		/*
		 * Temporarily mark page as recently-used to discourage
		 * SlruSelectLRUPage from selecting it again for someone else.
		 */
		SlruRecentlyUsed(shared, slotno);

		/* Release control lock while doing I/O */
		LWLockRelease(shared->ControlLock);

		/* Do the read */
		ok = SlruPhysicalReadPage(ctl, pageno, slotno);

		/* Set the LSNs for this newly read-in page to zero */
		SimpleLruZeroLSNs(ctl, slotno);

		/* Re-acquire control lock and update page state */
		LWLockAcquire(shared->ControlLock, LW_EXCLUSIVE);

		Assert(shared->page_number[slotno] == pageno &&
			   shared->page_status[slotno] == SLRU_PAGE_READ_IN_PROGRESS &&
			   !shared->page_dirty[slotno]);

		shared->page_status[slotno] = ok ? SLRU_PAGE_VALID : SLRU_PAGE_EMPTY;

		LWLockRelease(shared->buffer_locks[slotno]);

		/* Now it's okay to ereport if we failed */
		if (!ok && valid == NULL)
		   SlruReportIOError(ctl, pageno, xid);
		else if (valid != NULL)
		   {
		   if (!ok)
		     {
                     LWLockRelease(shared->ControlLock);
		     *valid = false;
                     return -1;
		     }
		   else
		     *valid = true;
		   }

		SlruRecentlyUsed(shared, slotno);
		return slotno;
	}
}

/*
 * Find a page in a shared buffer, reading it in if necessary.
 * The page number must correspond to an already-initialized page.
 * The caller must intend only read-only access to the page.
 *
 * The passed-in xid is used only for error reporting, and may be
 * InvalidTransactionId if no specific xid is associated with the action.
 *
 * Return value is the shared-buffer slot number now holding the page.
 * The buffer's LRU access info is updated.
 *
 * Control lock must NOT be held at entry, but will be held at exit.
 * It is unspecified whether the lock will be shared or exclusive.
 */
int
SimpleLruReadPage_ReadOnly(SlruCtl ctl, int pageno, TransactionId xid, bool *valid)
{
	SlruShared	shared = ctl->shared;
	int			slotno;

	/* Try to find the page while holding only shared lock */
	LWLockAcquire(shared->ControlLock, LW_SHARED);

	/* See if page is already in a buffer */
	for (slotno = 0; slotno < shared->num_slots; slotno++)
	{
		if (shared->page_number[slotno] == pageno &&
			shared->page_status[slotno] != SLRU_PAGE_EMPTY &&
			shared->page_status[slotno] != SLRU_PAGE_READ_IN_PROGRESS)
		{
			/* See comments for SlruRecentlyUsed macro */
			SlruRecentlyUsed(shared, slotno);
			if (valid != NULL)
				*valid = true;
			return slotno;
		}
	}

	/* No luck, so switch to normal exclusive lock and do regular read */
	LWLockRelease(shared->ControlLock);
	LWLockAcquire(shared->ControlLock, LW_EXCLUSIVE);

	return SimpleLruReadPage_Internal(ctl, pageno, true, xid, valid);
}

/*
 * Write a page from a shared buffer, if necessary.
 * Does nothing if the specified slot is not dirty.
 *
 * NOTE: only one write attempt is made here.  Hence, it is possible that
 * the page is still dirty at exit (if someone else re-dirtied it during
 * the write).	However, we *do* attempt a fresh write even if the page
 * is already being written; this is for checkpoints.
 *
 * Control lock must be held at entry, and will be held at exit.
 */
void
SimpleLruWritePage(SlruCtl ctl, int slotno, SlruFlush fdata)
{
	SlruShared	shared = ctl->shared;
	int			pageno = shared->page_number[slotno];
	bool		ok;

	/* If a write is in progress, wait for it to finish */
	while (shared->page_status[slotno] == SLRU_PAGE_WRITE_IN_PROGRESS &&
		   shared->page_number[slotno] == pageno)
	{
		SimpleLruWaitIO(ctl, slotno);
	}

	/*
	 * Do nothing if page is not dirty, or if buffer no longer contains the
	 * same page we were called for.
	 */
	if (!shared->page_dirty[slotno] ||
		shared->page_status[slotno] != SLRU_PAGE_VALID ||
		shared->page_number[slotno] != pageno)
		return;

	/*
	 * Mark the slot write-busy, and clear the dirtybit.  After this point, a
	 * transaction status update on this page will mark it dirty again.
	 */
	shared->page_status[slotno] = SLRU_PAGE_WRITE_IN_PROGRESS;
	shared->page_dirty[slotno] = false;

	/* Acquire per-buffer lock (cannot deadlock, see notes at top) */
	LWLockAcquire(shared->buffer_locks[slotno], LW_EXCLUSIVE);

	/* Release control lock while doing I/O */
	LWLockRelease(shared->ControlLock);

	/* Do the write */
	ok = SlruPhysicalWritePage(ctl, pageno, slotno, fdata);

	/* If we failed, and we're in a flush, better close the files */
	if (!ok && fdata)
	{
		int			i;

		for (i = 0; i < fdata->num_files; i++)
			MirroredFlatFile_Close(&fdata->mirroredOpens[i]);
	}

	/* Re-acquire control lock and update page state */
	LWLockAcquire(shared->ControlLock, LW_EXCLUSIVE);

	Assert(shared->page_number[slotno] == pageno &&
		   shared->page_status[slotno] == SLRU_PAGE_WRITE_IN_PROGRESS);

	/* If we failed to write, mark the page dirty again */
	if (!ok)
		shared->page_dirty[slotno] = true;

	shared->page_status[slotno] = SLRU_PAGE_VALID;

	LWLockRelease(shared->buffer_locks[slotno]);

	/* Now it's okay to ereport if we failed */
	if (!ok)
		SlruReportIOError(ctl, pageno, InvalidTransactionId);
}

/*
 * Generate the file name for flat file.
 */
static void
SlruFlatFileName(SlruCtl ctl, char *path, char *simpleFileName)
{
	char *dir = ctl->Dir;

	if (isTxnDir(ctl->Dir))
	{
		dir = makeRelativeToTxnFilespace(ctl->Dir);
	}

	if (snprintf(path, MAXPGPATH, "%s/%s", dir, simpleFileName) > MAXPGPATH)
	{
		ereport(ERROR, (errmsg("cannot generate path %s/%s", dir, simpleFileName)));
	}
}
/*
 * Physical read of a (previously existing) page into a buffer slot
 *
 * On failure, we cannot just ereport(ERROR) since caller has put state in
 * shared memory that must be undone.  So, we return FALSE and save enough
 * info in static variables to let SlruReportIOError make the report.
 *
 * For now, assume it's not worth keeping a file pointer open across
 * read/write operations.  We could cache one virtual file pointer ...
 */
static bool
SlruPhysicalReadPage(SlruCtl ctl, int pageno, int slotno)
{
	SlruShared	shared = ctl->shared;
	int			segno = pageno / SLRU_PAGES_PER_SEGMENT;
	int			rpageno = pageno % SLRU_PAGES_PER_SEGMENT;
	int			offset = rpageno * BLCKSZ;
	char		simpleFileName[MAXPGPATH];
	char		path[MAXPGPATH];
	int			fd;

	SlruSimpleFileName(simpleFileName, segno);
	SlruFlatFileName(ctl, path, simpleFileName);

	/*
	 * In a crash-and-restart situation, it's possible for us to receive
	 * commands to set the commit status of transactions whose bits are in
	 * already-truncated segments of the commit log (see notes in
	 * SlruPhysicalWritePage).	Hence, if we are InRecovery, allow the case
	 * where the file doesn't exist, and return zeroes instead.
	 */
	fd = BasicOpenFile(path, O_RDWR | PG_BINARY, S_IRUSR | S_IWUSR);
	if (fd < 0)
	{
		if (errno != ENOENT || !InRecovery)
		{
			slru_errcause = SLRU_OPEN_FAILED;
			slru_errno = errno;
			return false;
		}

		ereport(LOG,
				(errmsg("file \"%s\" doesn't exist, reading as zeroes",
						path)));
		MemSet(shared->page_buffer[slotno], 0, BLCKSZ);
		return true;
	}

	if (lseek(fd, (off_t) offset, SEEK_SET) < 0)
	{
		slru_errcause = SLRU_SEEK_FAILED;
		slru_errno = errno;
		close(fd);
		return false;
	}

	if (read(fd, shared->page_buffer[slotno], BLCKSZ) != BLCKSZ)
	{
		slru_errcause = SLRU_READ_FAILED;
		slru_errno = errno;
		close(fd);
		return false;
	}

	if (close(fd))
	{
		slru_errcause = SLRU_CLOSE_FAILED;
		slru_errno = errno;
		return false;
	}

	return true;
}

/*
 * Physical write of a page from a buffer slot
 *
 * On failure, we cannot just ereport(ERROR) since caller has put state in
 * shared memory that must be undone.  So, we return FALSE and save enough
 * info in static variables to let SlruReportIOError make the report.
 *
 * For now, assume it's not worth keeping a file pointer open across
 * independent read/write operations.  We do batch operations during
 * SimpleLruFlush, though.
 *
 * fdata is NULL for a standalone write, pointer to open-file info during
 * SimpleLruFlush.
 */
static bool
SlruPhysicalWritePage(SlruCtl ctl, int pageno, int slotno, SlruFlush fdata)
{
	SlruShared	shared = ctl->shared;
	int			segno = pageno / SLRU_PAGES_PER_SEGMENT;
	int			rpageno = pageno % SLRU_PAGES_PER_SEGMENT;
	int			offset = rpageno * BLCKSZ;
	char		simpleFileName[MAXPGPATH];

	MirroredFlatFileOpen	*existingMirroredOpen = NULL;
	MirroredFlatFileOpen	newMirroredOpen = MirroredFlatFileOpen_Init;
	MirroredFlatFileOpen	*useMirroredOpen = NULL;

	/*
	 * Honor the write-WAL-before-data rule, if appropriate, so that we do not
	 * write out data before associated WAL records.  This is the same action
	 * performed during FlushBuffer() in the main buffer manager.
	 */
	if (shared->group_lsn != NULL)
	{
		/*
		 * We must determine the largest async-commit LSN for the page. This
		 * is a bit tedious, but since this entire function is a slow path
		 * anyway, it seems better to do this here than to maintain a per-page
		 * LSN variable (which'd need an extra comparison in the
		 * transaction-commit path).
		 */
		XLogRecPtr	max_lsn;
		int			lsnindex,
					lsnoff;

		lsnindex = slotno * shared->lsn_groups_per_page;
		max_lsn = shared->group_lsn[lsnindex++];
		for (lsnoff = 1; lsnoff < shared->lsn_groups_per_page; lsnoff++)
		{
			XLogRecPtr	this_lsn = shared->group_lsn[lsnindex++];

			if (XLByteLT(max_lsn, this_lsn))
				max_lsn = this_lsn;
		}

		if (!XLogRecPtrIsInvalid(max_lsn))
		{
			/*
			 * As noted above, elog(ERROR) is not acceptable here, so if
			 * XLogFlush were to fail, we must PANIC.  This isn't much of a
			 * restriction because XLogFlush is just about all critical
			 * section anyway, but let's make sure.
			 */
			START_CRIT_SECTION();
			XLogFlush(max_lsn);
			END_CRIT_SECTION();
		}
	}

	/*
	 * During a Flush, we may already have the desired file open.
	 */
	if (fdata)
	{
		int			i;

		for (i = 0; i < fdata->num_files; i++)
		{
			if (fdata->segno[i] == segno)
			{
				existingMirroredOpen = &fdata->mirroredOpens[i];
				break;
			}
		}
	}

	if (existingMirroredOpen == NULL ||
		!MirroredFlatFile_IsActive(existingMirroredOpen))
	{
		/*
		 * If the file doesn't already exist, we should create it.  It is
		 * possible for this to need to happen when writing a page that's not
		 * first in its segment; we assume the OS can cope with that. (Note:
		 * it might seem that it'd be okay to create files only when
		 * SimpleLruZeroPage is called for the first page of a segment.
		 * However, if after a crash and restart the REDO logic elects to
		 * replay the log from a checkpoint before the latest one, then it's
		 * possible that we will get commands to set transaction status of
		 * transactions that have already been truncated from the commit log.
		 * Easiest way to deal with that is to accept references to
		 * nonexistent files here and in SlruPhysicalReadPage.)
		 *
		 * Note: it is possible for more than one backend to be executing this
		 * code simultaneously for different pages of the same file. Hence,
		 * don't use O_EXCL or O_TRUNC or anything like that.
		 */
		SlruSimpleFileName(simpleFileName, segno);
		if (MirroredFlatFile_Open(
						&newMirroredOpen,
						ctl->Dir,
						simpleFileName,
						O_RDWR | O_CREAT | PG_BINARY,
						S_IRUSR | S_IWUSR,
						/* suppressError */ true,
						/* atomic operation */ false,
						/*isMirrorRecovery */ false))
		{
			slru_errcause = SLRU_OPEN_FAILED;
			slru_errno = errno;
			return false;
		}

		if (fdata)
		{
			if (fdata->num_files < MAX_FLUSH_BUFFERS)
			{
				fdata->mirroredOpens[fdata->num_files] = newMirroredOpen;
				useMirroredOpen = &fdata->mirroredOpens[fdata->num_files];
				fdata->segno[fdata->num_files] = segno;
				fdata->num_files++;
			}
			else
			{
				/*
				 * In the unlikely event that we exceed MAX_FLUSH_BUFFERS,
				 * fall back to treating it as a standalone write.
				 */
				fdata = NULL;

				useMirroredOpen = &newMirroredOpen;
			}
		}
		else
			useMirroredOpen = &newMirroredOpen;
	
	}
	else
		useMirroredOpen = existingMirroredOpen;

	Assert(useMirroredOpen != NULL);

	if (MirroredFlatFile_SeekSet(
						useMirroredOpen,
						offset) != offset)
	{
		slru_errcause = SLRU_SEEK_FAILED;
		slru_errno = errno;
		if (!fdata)
			MirroredFlatFile_Close(useMirroredOpen);
		return false;
	}

	if (MirroredFlatFile_Write(
						useMirroredOpen,
						offset,
						shared->page_buffer[slotno], 
						BLCKSZ,
						/* suppressError */ true))
	{
		slru_errcause = SLRU_WRITE_FAILED;
		slru_errno = errno;
		if (!fdata)
			MirroredFlatFile_Close(useMirroredOpen);
		return false;
	}

	/*
	 * If not part of Flush, need to fsync now.  We assume this happens
	 * infrequently enough that it's not a performance issue.
	 */
	if (!fdata)
	{
		if (ctl->do_fsync && 
			MirroredFlatFile_Flush(
							useMirroredOpen,
							/* suppressError */ true))
		{
			slru_errcause = SLRU_FSYNC_FAILED;
			slru_errno = errno;
			MirroredFlatFile_Close(useMirroredOpen);
			return false;
		}

		// UNDONE: We don't have a suppressError for close...
		MirroredFlatFile_Close(useMirroredOpen);
	}

	return true;
}

/*
 * Issue the error message after failure of SlruPhysicalReadPage or
 * SlruPhysicalWritePage.  Call this after cleaning up shared-memory state.
 */
static void
SlruReportIOError(SlruCtl ctl, int pageno, TransactionId xid)
{
	int			segno = pageno / SLRU_PAGES_PER_SEGMENT;
	int			rpageno = pageno % SLRU_PAGES_PER_SEGMENT;
	int			offset = rpageno * BLCKSZ;
	char		path[MAXPGPATH];

	SlruFileName(ctl, path, segno);
	errno = slru_errno;
	switch (slru_errcause)
	{
		case SLRU_OPEN_FAILED:
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not access status of transaction %u", xid),
					 errdetail("Could not open file \"%s\": %m.", path)));
			break;
		case SLRU_SEEK_FAILED:
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not access status of transaction %u", xid),
				 errdetail("Could not seek in file \"%s\" to offset %u: %m.",
						   path, offset)));
			break;
		case SLRU_READ_FAILED:
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not access status of transaction %u", xid),
			   errdetail("Could not read from file \"%s\" at offset %u: %m.",
						 path, offset)));
			break;
		case SLRU_WRITE_FAILED:
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not access status of transaction %u", xid),
				errdetail("Could not write to file \"%s\" at offset %u: %m.",
						  path, offset)));
			break;
		case SLRU_FSYNC_FAILED:
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not access status of transaction %u", xid),
					 errdetail("Could not fsync file \"%s\": %m.",
							   path)));
			break;
		case SLRU_CLOSE_FAILED:
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not access status of transaction %u", xid),
					 errdetail("Could not close file \"%s\": %m.",
							   path)));
			break;
		default:
			/* can't get here, we trust */
			elog(ERROR, "unrecognized SimpleLru error cause: %d",
				 (int) slru_errcause);
			break;
	}
}

/*
 * Select the slot to re-use when we need a free slot.
 *
 * The target page number is passed because we need to consider the
 * possibility that some other process reads in the target page while
 * we are doing I/O to free a slot.  Hence, check or recheck to see if
 * any slot already holds the target page, and return that slot if so.
 * Thus, the returned slot is *either* a slot already holding the pageno
 * (could be any state except EMPTY), *or* a freeable slot (state EMPTY
 * or CLEAN).
 *
 * Control lock must be held at entry, and will be held at exit.
 */
static int
SlruSelectLRUPage(SlruCtl ctl, int pageno)
{
	SlruShared	shared = ctl->shared;

	/* Outer loop handles restart after I/O */
	for (;;)
	{
		int			slotno;
		int			cur_count;
		int			bestslot;
		int			best_delta;
		int			best_page_number;

		/* See if page already has a buffer assigned */
		for (slotno = 0; slotno < shared->num_slots; slotno++)
		{
			if (shared->page_number[slotno] == pageno &&
				shared->page_status[slotno] != SLRU_PAGE_EMPTY)
				return slotno;
		}

		/*
		 * If we find any EMPTY slot, just select that one. Else locate the
		 * least-recently-used slot to replace.
		 *
		 * Normally the page_lru_count values will all be different and so
		 * there will be a well-defined LRU page.  But since we allow
		 * concurrent execution of SlruRecentlyUsed() within
		 * SimpleLruReadPage_ReadOnly(), it is possible that multiple pages
		 * acquire the same lru_count values.  In that case we break ties by
		 * choosing the furthest-back page.
		 *
		 * In no case will we select the slot containing latest_page_number
		 * for replacement, even if it appears least recently used.
		 *
		 * Notice that this next line forcibly advances cur_lru_count to a
		 * value that is certainly beyond any value that will be in the
		 * page_lru_count array after the loop finishes.  This ensures that
		 * the next execution of SlruRecentlyUsed will mark the page newly
		 * used, even if it's for a page that has the current counter value.
		 * That gets us back on the path to having good data when there are
		 * multiple pages with the same lru_count.
		 */
		cur_count = (shared->cur_lru_count)++;
		best_delta = -1;
		bestslot = 0;			/* no-op, just keeps compiler quiet */
		best_page_number = 0;	/* ditto */
		for (slotno = 0; slotno < shared->num_slots; slotno++)
		{
			int			this_delta;
			int			this_page_number;

			if (shared->page_status[slotno] == SLRU_PAGE_EMPTY)
				return slotno;
			this_delta = cur_count - shared->page_lru_count[slotno];
			if (this_delta < 0)
			{
				/*
				 * Clean up in case shared updates have caused cur_count
				 * increments to get "lost".  We back off the page counts,
				 * rather than trying to increase cur_count, to avoid any
				 * question of infinite loops or failure in the presence of
				 * wrapped-around counts.
				 */
				shared->page_lru_count[slotno] = cur_count;
				this_delta = 0;
			}
			this_page_number = shared->page_number[slotno];
			if ((this_delta > best_delta ||
				 (this_delta == best_delta &&
				  ctl->PagePrecedes(this_page_number, best_page_number))) &&
				this_page_number != shared->latest_page_number)
			{
				bestslot = slotno;
				best_delta = this_delta;
				best_page_number = this_page_number;
			}
		}

		/*
		 * If the selected page is clean, we're set.
		 */
		if (shared->page_status[bestslot] == SLRU_PAGE_VALID &&
			!shared->page_dirty[bestslot])
			return bestslot;

		/*
		 * We need to wait for I/O.  Normal case is that it's dirty and we
		 * must initiate a write, but it's possible that the page is already
		 * write-busy, or in the worst case still read-busy.  In those cases
		 * we wait for the existing I/O to complete.
		 */
		if (shared->page_status[bestslot] == SLRU_PAGE_VALID)
			SimpleLruWritePage(ctl, bestslot, NULL);
		else
			SimpleLruWaitIO(ctl, bestslot);

		/*
		 * Now loop back and try again.  This is the easiest way of dealing
		 * with corner cases such as the victim page being re-dirtied while we
		 * wrote it.
		 */
	}
}

/*
 * Flush dirty pages to disk during checkpoint or database shutdown
 */
void
SimpleLruFlush(SlruCtl ctl, bool checkpoint)
{
	SlruShared	shared = ctl->shared;
	SlruFlushData fdata;
	int			slotno;
	int			pageno = 0;
	int			i;
	bool		ok;

	/*
	 * Find and write dirty pages
	 */
	fdata.num_files = 0;

	LWLockAcquire(shared->ControlLock, LW_EXCLUSIVE);

	for (slotno = 0; slotno < shared->num_slots; slotno++)
	{
		SimpleLruWritePage(ctl, slotno, &fdata);

		/*
		 * When called during a checkpoint, we cannot assert that the slot is
		 * clean now, since another process might have re-dirtied it already.
		 * That's okay.
		 */
		Assert(checkpoint ||
			   shared->page_status[slotno] == SLRU_PAGE_EMPTY ||
			   (shared->page_status[slotno] == SLRU_PAGE_VALID &&
				!shared->page_dirty[slotno]));
	}

	LWLockRelease(shared->ControlLock);

	/*
	 * Now fsync and close any files that were open
	 */
	ok = true;
	for (i = 0; i < fdata.num_files; i++)
	{
		if (ctl->do_fsync && 
			MirroredFlatFile_Flush(
							&fdata.mirroredOpens[i],
							/* suppressError */ true))
		{
			slru_errcause = SLRU_FSYNC_FAILED;
			slru_errno = errno;
			pageno = fdata.segno[i] * SLRU_PAGES_PER_SEGMENT;
			ok = false;
		}

		// UNDONE: We don't have a suppressError for close...
		MirroredFlatFile_Close(&fdata.mirroredOpens[i]);
	}
	if (!ok)
		SlruReportIOError(ctl, pageno, InvalidTransactionId);
}

/*
 * Remove all segments before the one holding the passed page number
 */
static void
SimpleLruTruncate_internal(SlruCtl ctl, int cutoffPage, bool lockHeld)
{
	SlruShared	shared = ctl->shared;
	int			slotno;

	/*
	 * The cutoff point is the start of the segment containing cutoffPage.
	 */
	cutoffPage -= cutoffPage % SLRU_PAGES_PER_SEGMENT;

	/*
	 * Scan shared memory and remove any pages preceding the cutoff page, to
	 * ensure we won't rewrite them later.  (Since this is normally called in
	 * or just after a checkpoint, any dirty pages should have been flushed
	 * already ... we're just being extra careful here.)
	 */
	if (!lockHeld)
		LWLockAcquire(shared->ControlLock, LW_EXCLUSIVE);

restart:;

	/*
	 * While we are holding the lock, make an important safety check: the
	 * planned cutoff point must be <= the current endpoint page. Otherwise we
	 * have already wrapped around, and proceeding with the truncation would
	 * risk removing the current segment.
	 */
	if (ctl->PagePrecedes(shared->latest_page_number, cutoffPage))
	{
		if (!lockHeld)
			LWLockRelease(shared->ControlLock);

		ereport(LOG,
				(errmsg("could not truncate directory \"%s\": apparent wraparound",
						ctl->Dir)));
		return;
	}

	for (slotno = 0; slotno < shared->num_slots; slotno++)
	{
		if (shared->page_status[slotno] == SLRU_PAGE_EMPTY)
			continue;
		if (!ctl->PagePrecedes(shared->page_number[slotno], cutoffPage))
			continue;

		/*
		 * If page is clean, just change state to EMPTY (expected case).
		 */
		if (shared->page_status[slotno] == SLRU_PAGE_VALID &&
			!shared->page_dirty[slotno])
		{
			shared->page_status[slotno] = SLRU_PAGE_EMPTY;
			continue;
		}

		/*
		 * Hmm, we have (or may have) I/O operations acting on the page, so
		 * we've got to wait for them to finish and then start again. This is
		 * the same logic as in SlruSelectLRUPage.	(XXX if page is dirty,
		 * wouldn't it be OK to just discard it without writing it?  For now,
		 * keep the logic the same as it was.)
		 */
		if (shared->page_status[slotno] == SLRU_PAGE_VALID)
			SimpleLruWritePage(ctl, slotno, NULL);
		else
			SimpleLruWaitIO(ctl, slotno);
		goto restart;
	}

	if (!lockHeld)
		LWLockRelease(shared->ControlLock);

	/* Now we can remove the old segment(s) */
	(void) SlruScanDirectory(ctl, cutoffPage, true);
}
void
SimpleLruTruncate(SlruCtl ctl, int cutoffPage)
{
	SimpleLruTruncate_internal(ctl, cutoffPage, false);
}
/* Like SimpleLruTruncate, but we're already holding the control lock */
void
SimpleLruTruncateWithLock(SlruCtl ctl, int cutoffPage)
{
	SimpleLruTruncate_internal(ctl, cutoffPage, true);
}

/*
 * SimpleLruTruncate subroutine: scan directory for removable segments.
 * Actually remove them iff doDeletions is true.  Return TRUE iff any
 * removable segments were found.  Note: no locking is needed.
 *
 * This can be called directly from clog.c, for reasons explained there.
 */
bool
SlruScanDirectory(SlruCtl ctl, int cutoffPage, bool doDeletions)
{
	bool		found = false;
	DIR		   *cldir;
	struct dirent *clde;
	int			segno;
	int			segpage;
	char		path[MAXPGPATH];
	char		*dir = NULL;
	char		*mirrorDir = NULL;

	/*
	 * The cutoff point is the start of the segment containing cutoffPage.
	 * (This is redundant when called from SimpleLruTruncate, but not when
	 * called directly from clog.c.)
	 */
	cutoffPage -= cutoffPage % SLRU_PAGES_PER_SEGMENT;

	/* 
	 * PG_SUBTRANS is initialized with the default directory. Make sure
	 * it is relative to the current transaction filespace
	 */
	if (isTxnDir(ctl->Dir))
	{
		dir = makeRelativeToTxnFilespace(ctl->Dir);
		mirrorDir = makeRelativeToPeerTxnFilespace(ctl->Dir);
	}
	else
	{
		dir = (char*)palloc(MAXPGPATH);
		strncpy(dir, ctl->Dir, MAXPGPATH);
		mirrorDir = (char*)palloc(MAXPGPATH);
		strncpy(mirrorDir, ctl->Dir, MAXPGPATH);
	}

	cldir = AllocateDir(dir);
	while ((clde = ReadDir(cldir, dir)) != NULL)
	{
		if (isSlruFileName(clde->d_name))
		{
			segno = (int) strtol(clde->d_name, NULL, 16);
			segpage = segno * SLRU_PAGES_PER_SEGMENT;
			if (ctl->PagePrecedes(segpage, cutoffPage))
			{
				found = true;
				if (doDeletions)
				{
					if (snprintf(path, MAXPGPATH, "%s/%s", dir, clde->d_name) > MAXPGPATH)
					{
						ereport(ERROR, (errmsg("cannot form path %s/%s", dir, clde->d_name)));
					}
					ereport(DEBUG2,
							(errmsg("removing file \"%s\"", path)));

					// UNDONE: Old code ignored errors...
					MirroredFlatFile_Drop(
									ctl->Dir,
									clde->d_name,
									/* suppressError */ true,
									/*isMirrorRecovery */ false);
				}
			}
		}
	}
	FreeDir(cldir);

	pfree(dir);
	pfree(mirrorDir);

	return found;
}

/*
 * Test if a page exists.
 */
bool
SimpleLruPageExists(SlruCtl ctl, int pageno)
{
	SlruShared	shared = ctl->shared;

	/* Outer loop handles restart if we must wait for someone else's I/O */
	for (;;)
	{
		int			slotno;
		bool		ok;

		/* See if page already is in memory; if not, pick victim slot */
		slotno = SlruSelectLRUPage(ctl, pageno);

		/* Did we find the page in memory? */
		if (shared->page_number[slotno] == pageno &&
			shared->page_status[slotno] != SLRU_PAGE_EMPTY)
		{
			/* If page is still being read in, we must wait for I/O */
			if (shared->page_status[slotno] == SLRU_PAGE_READ_IN_PROGRESS)
			{
				SimpleLruWaitIO(ctl, slotno);
				/* Now we must recheck state from the top */
				continue;
			}
			/* Otherwise, exists */
			return true;
		}

		/* We found no match; assert we selected a freeable slot */
		Assert(shared->page_status[slotno] == SLRU_PAGE_EMPTY ||
			   (shared->page_status[slotno] == SLRU_PAGE_VALID &&
				!shared->page_dirty[slotno]));

		/* Mark the slot read-busy */
		shared->page_number[slotno] = pageno;
		shared->page_status[slotno] = SLRU_PAGE_READ_IN_PROGRESS;
		shared->page_dirty[slotno] = false;

		/* Acquire per-buffer lock (cannot deadlock, see notes at top) */
		LWLockAcquire(shared->buffer_locks[slotno], LW_EXCLUSIVE);

		/*
		 * Temporarily mark page as recently-used to discourage
		 * SlruSelectLRUPage from selecting it again for someone else.
		 */
		SlruRecentlyUsed(shared, slotno);

		/* Release control lock while doing I/O */
		LWLockRelease(shared->ControlLock);

		/* Do the read */
		ok = SlruPhysicalReadPage(ctl, pageno, slotno);

		/* Re-acquire control lock and update page state */
		LWLockAcquire(shared->ControlLock, LW_EXCLUSIVE);

		Assert(shared->page_number[slotno] == pageno &&
			   shared->page_status[slotno] == SLRU_PAGE_READ_IN_PROGRESS &&
			   !shared->page_dirty[slotno]);

		shared->page_status[slotno] = ok ? SLRU_PAGE_VALID : SLRU_PAGE_EMPTY;

		LWLockRelease(shared->buffer_locks[slotno]);

		return ok;
	}

	return false;	// Should not reach here.
}

/*
 * This externally visible function will copy several directories from the
 * primary segment to the mirror segment, if needed.
 */
int
SlruRecoverMirror(void)
{
	int retval;

	elog(LOG, "recovering %s", CLOG_DIR);
	retval = SlruRecoverMirrorDir(CLOG_DIR);

	if (retval != 0)
		return retval;

	elog(LOG, "recovering %s", DISTRIBUTEDLOG_DIR);
	retval = SlruRecoverMirrorDir(DISTRIBUTEDLOG_DIR);

	if (retval != 0)
		return retval;

	elog(LOG, "recovering %s", DISTRIBUTEDXIDMAP_DIR);
	retval = SlruRecoverMirrorDir(DISTRIBUTEDXIDMAP_DIR);

	if (retval != 0)
		return retval;

	elog(LOG, "recovering %s", MULTIXACT_MEMBERS_DIR);
	retval = SlruRecoverMirrorDir(MULTIXACT_MEMBERS_DIR);

	if (retval != 0)
		return retval;

	elog(LOG, "recovering %s", MULTIXACT_OFFSETS_DIR);
	retval = SlruRecoverMirrorDir(MULTIXACT_OFFSETS_DIR);

	if (retval != 0)
		return retval;

	elog(LOG, "recovering %s", SUBTRANS_DIR);
	retval = SlruRecoverMirrorDir(SUBTRANS_DIR);

	return retval;
}

/*
 * This function will check if the checksum of all the files in 'dirName' match
 * with those on the mirror and transfer the files if the checksums don't match.
 */
static int
SlruRecoverMirrorDir(char *dirName)
{
	char *fullDirName = NULL;
	int	 retval = STATUS_OK;

	if (isTxnDir(dirName))
	{
		fullDirName = makeRelativeToTxnFilespace(dirName);
	}
	else
	{
		fullDirName = (char*)palloc(MAXPGPATH);
		strncpy(fullDirName, dirName, MAXPGPATH);
	}

	retval = SlruVerifyDirectoryChecksum(fullDirName);

	/*
	 * If checksum mismatch, copy all files in the directory from the
	 * primary to the mirror.
	 */
	if (retval != STATUS_OK)
		retval = SlruCopyDirectory(dirName, fullDirName);

	pfree(fullDirName);

	return retval;
}

/*
 * Verify checksum of a primary directory wrt. to the corresponding mirror
 * directory.
 */
static int
SlruVerifyDirectoryChecksum(char *fullDirName)
{
	char checksumFilePath[MAXPGPATH];
	char md5[SLRU_MD5_BUFLEN] = {0};
	int retval = STATUS_OK;

	snprintf(checksumFilePath, sizeof(checksumFilePath), "%s/%s", fullDirName,
			 SLRU_CHECKSUM_FILENAME);

	/*
	 * We generate the checksum file and then compute its checksum in an
	 * SlruComputeChecksum() call.  We keep the checksum file so that if needed
	 * support can diff the checksum files at the primary and the mirror to see
	 * which file(s) were not in sync.
	 */
	retval = FileRepPrimary_MirrorStartChecksum(
		FileRep_GetFlatFileIdentifier(fullDirName, SLRU_CHECKSUM_FILENAME));

	if (retval != STATUS_OK)
	{
		ereport(WARNING,
				(errmsg("FileRepPrimary_MirrorStartChecksum() returned: %d",
						retval)));
		return retval;
	}

	retval = SlruCreateChecksumFile(fullDirName);

	if (retval != STATUS_OK)
		return retval;


	retval = SlruComputeChecksum(checksumFilePath, md5);

	if (retval != STATUS_OK)
		return retval;

	retval = FileRepPrimary_MirrorVerifyDirectoryChecksum(
				FileRep_GetFlatFileIdentifier(fullDirName, SLRU_CHECKSUM_FILENAME), md5);

	if (retval != STATUS_OK)
		ereport(WARNING,
				(errmsg("FileRepPrimary_MirrorVerifyDirectoryChecksum() returned: %d",
						retval)));

	return retval;
}

/*
 * Create a checksum file called 'slru_checksum_file' in the directory
 * specified by 'fullDirName'.
 */
int
SlruCreateChecksumFile(const char *fullDirName)
{
	DIR			  *slruDir = NULL;
	struct dirent *dirEntry;
	char 		  *fileName;
	char		   filePath[MAXPGPATH];
	char		   checksumFilePath[MAXPGPATH];
	File		   checksumFileHandle = 0;
	int			   retval = STATUS_OK;
	char		   buf[SLRU_CKSUM_LINE_LEN];

	snprintf(checksumFilePath, sizeof(checksumFilePath), "%s/%s", fullDirName,
			 SLRU_CHECKSUM_FILENAME);

	checksumFileHandle = PathNameOpenFile(checksumFilePath, O_CREAT | O_TRUNC | O_WRONLY,
										  S_IRUSR | S_IWUSR);
	if (checksumFileHandle < 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("could not open file \"%s\": %m", checksumFilePath)));
		return STATUS_ERROR;
	}

	slruDir = AllocateDir(fullDirName);
	if (!slruDir)
	{
		FileClose(checksumFileHandle);
		return STATUS_ERROR;
	}

	while ((dirEntry = ReadDir(slruDir, fullDirName)) != NULL)
	{
		char  md5[SLRU_MD5_BUFLEN] = {0};

		fileName = dirEntry->d_name;

		if (isSlruFileName(fileName))
		{
			snprintf(filePath, sizeof(filePath), "%s/%s", fullDirName, fileName);

			if (SlruComputeChecksum(filePath, md5) < 0)
			{
				ereport(WARNING,
						(errmsg("could not compute checksum for file %s: %m",
								filePath)));
				retval = STATUS_ERROR;
				break;

			}

			snprintf(buf, sizeof(buf), "%s: %s\n", fileName, md5);

			if (FileWrite(checksumFileHandle, buf, strlen(buf)) < 0)
			{
				ereport(WARNING,
						(errmsg("could not write to checksum file %s: %m",
								checksumFilePath)));
				retval = STATUS_ERROR;
				break;

			}
		}
	}

	FreeDir(slruDir);
	FileClose(checksumFileHandle);

	return retval;
}

/*
 * Given a filename, this function will return true if and only if it is a valid
 * SLRU filename. Filenames with 4 hex characters are valid.
 */
static bool
isSlruFileName(const char *fileName)
{
	return (strlen(fileName) == SLRU_FILENAME_LEN &&
			strspn(fileName, "0123456789ABCDEF") == SLRU_FILENAME_LEN);
}

/*
 * Compute the md5 hash of the file specified by 'filePath'.
 */
static int
SlruComputeChecksum(char *filePath, char *md5)
{
	File fileHandle = 0;
	int  retval = STATUS_OK;
	char buf[BLCKSZ * SLRU_PAGES_PER_SEGMENT];
	int  bytesRead;

	fileHandle = PathNameOpenFile(filePath, O_RDONLY | PG_BINARY, S_IRUSR);
	if (fileHandle < 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("could not open file %s: %m", filePath)));
		return STATUS_ERROR;
	}

	bytesRead = FileRead(fileHandle, buf, sizeof(buf));
	if (bytesRead >= 0)
		pg_md5_hash(buf, bytesRead, md5);
	else
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("could not read file %s: %m", filePath)));
		retval = STATUS_ERROR;
	}

	FileClose(fileHandle);

	return retval;
}

/*
 * Copy all the files from the fullDirName to the corresponding directory at the
 * mirror.
 */
static int
SlruCopyDirectory(char *dirName, char *fullDirName)
{
	DIR			  *slruDir = NULL;
	struct dirent *dirEntry;
	int			   retval = STATUS_OK;
	int			   counter = 0;

	slruDir = AllocateDir(fullDirName);
	if (!slruDir)
		return STATUS_ERROR;

	while ((dirEntry = ReadDir(slruDir, fullDirName)) != NULL)
	{
		if (isSlruFileName(dirEntry->d_name))
		{
			retval = MirrorFlatFile(dirName, dirEntry->d_name);

			if (retval != 0)
				break;

			counter++;

			if (counter % log_count_recovered_files_batch == 0)
			{
				elog(LOG, "completed recovering %d files for directory %s",
					 counter, dirName);
			}
		}
	}

	if (retval == 0)
		elog(LOG, "completed recovering %d files for directory %s", counter,
			 dirName);
	else
		elog(WARNING,
			 "could not copy all the files for directory %s (files copied: %d)",
			 dirName, counter);

	FreeDir(slruDir);

	return retval;
}

/*
 * This function is called from the mirror to compute the checksum of the
 * mirror's checksum file and compare the mirror's checksum with that of the
 * primary (variable 'primaryMd5').
 */
int
SlruMirrorVerifyDirectoryChecksum(char *dirName, char *checksumFile,
								  char *primaryMd5)
{
	int  retval = STATUS_OK;
	char mirrorMd5[SLRU_MD5_BUFLEN] = {0};
	char filePath[MAXPGPATH];

	snprintf(filePath, sizeof(filePath), "%s/%s", dirName, checksumFile);

	if (SlruComputeChecksum(filePath, mirrorMd5) < 0)
	{
		ereport(WARNING,
				(errmsg("could not compute checksum for file %s/%s: %m",
						dirName, filePath)));
		retval = STATUS_ERROR;
	}
	else if (memcmp(primaryMd5, mirrorMd5, sizeof(mirrorMd5)))
	{
		ereport(WARNING,
				(errmsg("checksum mismatch for file: %s/%s",
						dirName, checksumFile)));
		retval = STATUS_ERROR;
	}

	return retval;
}
