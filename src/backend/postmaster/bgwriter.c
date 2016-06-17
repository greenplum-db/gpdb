/*-------------------------------------------------------------------------
 *
 * bgwriter.c
 *
 * The background writer (bgwriter) is new as of Postgres 8.0.	It attempts
 * to keep regular backends from having to write out dirty shared buffers
 * (which they would only do when needing to free a shared buffer to read in
 * another page).  In the best scenario all writes from shared buffers will
 * be issued by the background writer process.	However, regular backends are
 * still empowered to issue writes if the bgwriter fails to maintain enough
 * clean shared buffers.
 *
 * Previously, the background writer use to do this duty.  But we ended up with
 * a deadlock with the new FileRep functionality, so this functionality was split out into its
 * own server.
 *
 * The bgwriter is started by the postmaster as soon as the startup subprocess
 * finishes.  It remains alive until the postmaster commands it to terminate.
 * Normal termination is by SIGUSR2, which instructs the bgwriter to execute
 * a shutdown checkpoint and then exit(0).	(All backends must be stopped
 * before SIGUSR2 is issued!)  Emergency termination is by SIGQUIT; like any
 * backend, the bgwriter will simply abort and exit on SIGQUIT.
 *
 * If the bgwriter exits unexpectedly, the postmaster treats that the same
 * as a backend crash: shared memory may be corrupted, so remaining backends
 * should be killed by SIGQUIT and then a recovery cycle started.  (Even if
 * shared memory isn't corrupted, we have lost information about which
 * files need to be fsync'd for the next checkpoint, and so a system
 * restart needs to be forced.)
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/postmaster/bgwriter.c,v 1.48 2008/01/01 19:45:51 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "access/xlog_internal.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/bgwriter.h"
#include "storage/fd.h"
#include "storage/freespace.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/pmsignal.h"
#include "storage/shmem.h"
#include "storage/smgr.h"
#include "storage/spin.h"
#include "tcop/tcopprot.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/resowner.h"
#include "utils/faultinjector.h"

#include "tcop/tcopprot.h" /* quickdie() */

/*----------
 * Shared memory area for communication between bgwriter and backends
 *
 * num_backend_writes is used to count the number of buffer writes performed
 * by non-bgwriter processes.  This counter should be wide enough that it
 * can't overflow during a single bgwriter cycle.
 *
 * The requests array holds fsync requests sent by backends and not yet
 * absorbed by the bgwriter.
 *
 * Unlike the checkpoint fields, num_backend_writes and the requests
 * fields are protected by BgWriterCommLock.
 *----------
 */
typedef struct
{
	RelFileNode rnode;
	BlockNumber segno;			/* see md.c for special values */
	/* might add a real request-type field later; not needed yet */
} BgWriterRequest;

typedef struct
{
	pid_t		bgwriter_pid;	/* PID of bgwriter (0 if not started) */

	uint32		num_backend_writes;		/* counts non-bgwriter buffer writes */

	int			num_requests;	/* current # of requests */
	int			max_requests;	/* allocated array size */
	BgWriterRequest requests[1];	/* VARIABLE LENGTH ARRAY */
} BgWriterShmemStruct;

static BgWriterShmemStruct *BgWriterShmem;

/*
 * GUC parameters
 */
int			BgWriterDelay = 200;

/*
 * Flags set by interrupt handlers for later service in the main loop.
 */
static volatile sig_atomic_t got_SIGHUP = false;
static volatile sig_atomic_t checkpoint_smgrcloseall_requested = false;
static volatile sig_atomic_t shutdown_requested = false;

/*
 * Private state
 */
static bool am_bg_writer = false;
static time_t last_xlog_switch_time;

/* Prototypes for private functions */

static void BgWriterNap(void);
static void CheckArchiveTimeout(void);
static bool CompactBgwriterRequestQueue(void);

/* Signal handlers */

static void BgSigHupHandler(SIGNAL_ARGS);
static void ReqCheckpointSmgrCloseHandler(SIGNAL_ARGS);
static void ReqShutdownHandler(SIGNAL_ARGS);

/*
 * Main entry point for bgwriter process
 *
 * This is invoked from AuxiliaryProcessMain, which has already created the basic
 * execution environment, but not enabled signals yet.
 */
void
BackgroundWriterMain(void)
{
	sigjmp_buf	local_sigjmp_buf;
	MemoryContext bgwriter_context;

	BgWriterShmem->bgwriter_pid = MyProcPid;
	am_bg_writer = true;

	/*
	 * If possible, make this process a group leader, so that the postmaster
	 * can signal any child processes too.	(bgwriter probably never has any
	 * child processes, but for consistency we make all postmaster child
	 * processes do this.)
	 */
#ifdef HAVE_SETSID
	if (setsid() < 0)
		elog(FATAL, "setsid() failed: %m");
#endif

	/*
	 * Properly accept or ignore signals the postmaster might send us
	 *
	 * Note: we deliberately ignore SIGTERM, because during a standard Unix
	 * system shutdown cycle, init will SIGTERM all processes at once.	We
	 * want to wait for the backends to exit, whereupon the postmaster will
	 * tell us it's okay to shut down (via SIGUSR2).
	 *
	 * SIGUSR1 is presently unused; keep it spare in case someday we want this
	 * process to participate in ProcSignal messaging.
	 */
	pqsignal(SIGHUP, BgSigHupHandler);	/* set flag to read config file */
	pqsignal(SIGINT, ReqCheckpointSmgrCloseHandler);		/* request checkpoint smgrcloseall */
	pqsignal(SIGTERM, SIG_IGN); /* ignore SIGTERM */
	pqsignal(SIGQUIT, quickdie);		/* hard crash time: nothing bg-writer specific, just use the standard */
	pqsignal(SIGALRM, SIG_IGN);
	pqsignal(SIGPIPE, SIG_IGN);
	pqsignal(SIGUSR1, SIG_IGN); /* reserve for ProcSignal */
	pqsignal(SIGUSR2, ReqShutdownHandler);		/* request shutdown */

	/*
	 * Reset some signals that are accepted by postmaster but not here
	 */
	pqsignal(SIGCHLD, SIG_DFL);
	pqsignal(SIGTTIN, SIG_DFL);
	pqsignal(SIGTTOU, SIG_DFL);
	pqsignal(SIGCONT, SIG_DFL);
	pqsignal(SIGWINCH, SIG_DFL);

	/* We allow SIGQUIT (quickdie) at all times */
#ifdef HAVE_SIGPROCMASK
	sigdelset(&BlockSig, SIGQUIT);
#else
	BlockSig &= ~(sigmask(SIGQUIT));
#endif

	/*
	 * Initialize so that first time-driven event happens at the correct time.
	 */
	last_xlog_switch_time = time(NULL);

	/*
	 * Create a resource owner to keep track of our resources (currently only
	 * buffer pins).
	 */
	CurrentResourceOwner = ResourceOwnerCreate(NULL, "Background Writer");

	/*
	 * Create a memory context that we will do all our work in.  We do this so
	 * that we can reset the context during error recovery and thereby avoid
	 * possible memory leaks.  Formerly this code just ran in
	 * TopMemoryContext, but resetting that would be a really bad idea.
	 */
	bgwriter_context = AllocSetContextCreate(TopMemoryContext,
											 "Background Writer",
											 ALLOCSET_DEFAULT_MINSIZE,
											 ALLOCSET_DEFAULT_INITSIZE,
											 ALLOCSET_DEFAULT_MAXSIZE);
	MemoryContextSwitchTo(bgwriter_context);

	/*
	 * If an exception is encountered, processing resumes here.
	 *
	 * See notes in postgres.c about the design of this coding.
	 */
	if (sigsetjmp(local_sigjmp_buf, 1) != 0)
	{
		/* Since not using PG_TRY, must reset error stack by hand */
		error_context_stack = NULL;

		/* Prevent interrupts while cleaning up */
		HOLD_INTERRUPTS();

		/* Report the error to the server log */
		EmitErrorReport();

		/*
		 * These operations are really just a minimal subset of
		 * AbortTransaction().	We don't have very many resources to worry
		 * about in bgwriter, but we do have LWLocks, buffers, and temp files.
		 */
		LWLockReleaseAll();
		AbortBufferIO();
		UnlockBuffers();
		/* buffer pins are released here: */
		ResourceOwnerRelease(CurrentResourceOwner,
							 RESOURCE_RELEASE_BEFORE_LOCKS,
							 false, true);
		/* we needn't bother with the other ResourceOwnerRelease phases */
		AtEOXact_Buffers(false);
		AtEOXact_Files();
		AtEOXact_HashTables(false);

		/*
		 * Now return to normal top-level context and clear ErrorContext for
		 * next time.
		 */
		MemoryContextSwitchTo(bgwriter_context);
		FlushErrorState();

		/* Flush any leaked data in the top-level context */
		MemoryContextResetAndDeleteChildren(bgwriter_context);

		/* Now we can allow interrupts again */
		RESUME_INTERRUPTS();

		/*
		 * Sleep at least 1 second after any error.  A write error is likely
		 * to be repeated, and we don't want to be filling the error logs as
		 * fast as we can.
		 */
		pg_usleep(1000000L);

		/*
		 * Close all open files after any error.  This is helpful on Windows,
		 * where holding deleted files open causes various strange errors.
		 * It's not clear we need it elsewhere, but shouldn't hurt.
		 */
		smgrcloseall();
	}

	/* We can now handle ereport(ERROR) */
	PG_exception_stack = &local_sigjmp_buf;

	/*
	 * Unblock signals (they were blocked when the postmaster forked us)
	 */
	PG_SETMASK(&UnBlockSig);

	/*
	 * Loop forever
	 */
	for (;;)
	{
		bool		do_checkpoint_smgrcloseall = false;

		/*
		 * Emergency bailout if postmaster has died.  This is to avoid the
		 * necessity for manual cleanup of all postmaster children.
		 */
		if (!PostmasterIsAlive(true))
			exit(1);

#ifdef USE_ASSERT_CHECKING
#ifdef FAULT_INJECTOR
    FaultInjector_InjectFaultIfSet(
    		FaultInBackgroundWriterMain,
            DDLNotSpecified,
            "",  // databaseName
            ""); // tableName
#endif
#endif
		/*
		 * Process any requests or signals received recently.
		 */
		AbsorbFsyncRequests();

		if (got_SIGHUP)
		{
			got_SIGHUP = false;
			ProcessConfigFile(PGC_SIGHUP);
		}
		if (checkpoint_smgrcloseall_requested)
		{
			checkpoint_smgrcloseall_requested = false;
			do_checkpoint_smgrcloseall = true;
		}
		if (shutdown_requested)
		{
			/*
			 * From here on, elog(ERROR) should end with exit(1), not send
			 * control back to the sigsetjmp block above
			 */
			ExitOnAnyError = true;
			/* Close down the database */
			ShutdownXLOG(0, 0);
			DumpFreeSpaceMap(0, 0);

			/* Normal exit from the bgwriter server is here */
			if (Debug_print_server_processes ||
				Debug_print_qd_mirroring ||
				log_min_messages <= DEBUG5)
			{
				ereport((log_min_messages > DEBUG5 ? LOG : DEBUG5),
						(errmsg("normal exit from background writer")));
			}
			proc_exit(0);		/* done */
		}

		/*
		 * Do a checkpoint smgrcloseall if requested, otherwise do one cycle of
		 * dirty-buffer writing.
		 */
		if (do_checkpoint_smgrcloseall)
		{
			/*
			 * After any checkpoint, close all smgr files.	This is so we
			 * won't hang onto smgr references to deleted files indefinitely.
			 */
			smgrcloseall();
		}
		else
			BgBufferSync();

		/* Check for archive_timeout and switch xlog files if necessary. */
		CheckArchiveTimeout();

		/* Nap for the configured time. */
		BgWriterNap();
	}
}

/*
 * CheckArchiveTimeout -- check for archive_timeout and switch xlog files
 *		if needed
 */
static void
CheckArchiveTimeout(void)
{
	time_t		now;
	time_t		last_time;

	if (XLogArchiveTimeout <= 0)
		return;

	now = time(NULL);

	/* First we do a quick check using possibly-stale local state. */
	if ((int) (now - last_xlog_switch_time) < XLogArchiveTimeout)
		return;

	/*
	 * Update local state ... note that last_xlog_switch_time is the last time
	 * a switch was performed *or requested*.
	 */
	last_time = GetLastSegSwitchTime();

	last_xlog_switch_time = Max(last_xlog_switch_time, last_time);

	/* Now we can do the real check */
	if ((int) (now - last_xlog_switch_time) >= XLogArchiveTimeout)
	{
		XLogRecPtr	switchpoint;

		/* OK, it's time to switch */
		switchpoint = RequestXLogSwitch();

		/*
		 * If the returned pointer points exactly to a segment boundary,
		 * assume nothing happened.
		 */
		if ((switchpoint.xrecoff % XLogSegSize) != 0)
			ereport(DEBUG1,
				(errmsg("transaction log switch forced (archive_timeout=%d)",
						XLogArchiveTimeout)));

		/*
		 * Update state in any case, so we don't retry constantly when the
		 * system is idle.
		 */
		last_xlog_switch_time = now;
	}
}

/*
 * BgWriterNap -- Nap for the configured time or until a signal is received.
 */
void
BgWriterNap(void)
{
	long		udelay;

	/*
	 * Send off activity statistics to the stats collector
	 */
	pgstat_send_bgwriter();

	/*
	 * Nap for the configured time, or sleep for 10 seconds if there is no
	 * bgwriter activity configured.
	 *
	 * On some platforms, signals won't interrupt the sleep.  To ensure we
	 * respond reasonably promptly when someone signals us, break down the
	 * sleep into 1-second increments, and check for interrupts after each
	 * nap.
	 *
	 * We absorb pending requests after each short sleep.
	 */
	if (bgwriter_lru_maxpages > 0)
		udelay = BgWriterDelay * 1000L;
	else if (XLogArchiveTimeout > 0)
		udelay = 1000000L;		/* One second */
	else
		udelay = 10000000L;		/* Ten seconds */

	while (udelay > 999999L)
	{
		if (got_SIGHUP || shutdown_requested)
			break;
		pg_usleep(1000000L);
		AbsorbFsyncRequests();
		udelay -= 1000000L;
	}

	if (!(got_SIGHUP || shutdown_requested))
		pg_usleep(udelay);
}

/* --------------------------------
 *		signal handler routines
 * --------------------------------
 */

/* SIGHUP: set flag to re-read config file at next convenient time */
static void
BgSigHupHandler(SIGNAL_ARGS)
{
	got_SIGHUP = true;
}

/* SIGINT: set flag to run a normal checkpoint right away */
static void
ReqCheckpointSmgrCloseHandler(SIGNAL_ARGS)
{
	checkpoint_smgrcloseall_requested = true;
}

/* SIGUSR2: set flag to run a shutdown checkpoint and exit */
static void
ReqShutdownHandler(SIGNAL_ARGS)
{
	shutdown_requested = true;
}


/* --------------------------------
 *		communication with backends
 * --------------------------------
 */

/*
 * BgWriterShmemSize
 *		Compute space needed for bgwriter-related shared memory
 */
Size
BgWriterShmemSize(void)
{
	Size		size;

	/*
	 * Currently, the size of the requests[] array is arbitrarily set equal to
	 * NBuffers.  This may prove too large or small ...
	 */
	size = offsetof(BgWriterShmemStruct, requests);
	size = add_size(size, mul_size(NBuffers, sizeof(BgWriterRequest)));

	return size;
}

/*
 * BgWriterShmemInit
 *		Allocate and initialize bgwriter-related shared memory
 */
void
BgWriterShmemInit(void)
{
	Size		size = BgWriterShmemSize();
	bool		found;

	BgWriterShmem = (BgWriterShmemStruct *)
		ShmemInitStruct("Background Writer Data",
						size,
						&found);
	if (BgWriterShmem == NULL)
		ereport(FATAL,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("not enough shared memory for background writer")));

	if (!found)
	{
		/*
		 * First time through, so initialize.  Note that we zero the whole
		 * requests array; this is so that CompactBgwriterRequestQueue
		 * can assume that any pad bytes in the request structs are zeroes.
		 */
		MemSet(BgWriterShmem, 0, size);
		BgWriterShmem->max_requests = NBuffers;
	}
}

/*
 * RequestCheckpointSmgrCloseAll
 *		Called in checkpoint server process to request the background writer do a smgrcloseall.
 */
void
RequestCheckpointSmgrCloseAll(void)
{
	/*
	 * Send signal to request checkpoint.  When not waiting, we consider
	 * failure to send the signal to be nonfatal.
	 */
	if (BgWriterShmem->bgwriter_pid == 0)
		elog(LOG,
			 "could not request checkpoint close all because bgwriter not running");
	if (kill(BgWriterShmem->bgwriter_pid, SIGINT) != 0)
		elog(LOG,
			 "could not signal bgwriter for checkpoint close all: %m");
}

/*
 * ForwardFsyncRequest
 *		Forward a file-fsync request from a backend to the bgwriter
 *
 * Whenever a backend is compelled to write directly to a relation
 * (which should be seldom, if the bgwriter is getting its job done),
 * the backend calls this routine to pass over knowledge that the relation
 * is dirty and must be fsync'd before next checkpoint.  We also use this
 * opportunity to count such writes for statistical purposes.
 *
 * segno specifies which segment (not block!) of the relation needs to be
 * fsync'd.  (Since the valid range is much less than BlockNumber, we can
 * use high values for special flags; that's all internal to md.c, which
 * see for details.)
 *
 * To avoid holding the lock for longer than necessary, we normally write
 * to the requests[] queue without checking for duplicates.  The bgwriter
 * will have to eliminate dups internally anyway.  However, if we discover
 * that the queue is full, we make a pass over the entire queue to compact
 * it.  This is somewhat expensive, but the alternative is for the backend
 * to perform its own fsync, which is far more expensive in practice.  It
 * is theoretically possible a backend fsync might still be necessary, if
 * the queue is full and contains no duplicate entries.  In that case, we
 * let the backend know by returning false.
 */
bool
ForwardFsyncRequest(RelFileNode rnode, BlockNumber segno)
{
	BgWriterRequest *request;

	if (!IsUnderPostmaster)
		return false;			/* probably shouldn't even get here */

	if (am_bg_writer)
		elog(ERROR, "ForwardFsyncRequest must not be called in bgwriter");

	LWLockAcquire(BgWriterCommLock, LW_EXCLUSIVE);

	/* we count non-bgwriter writes even when the request queue overflows */
	BgWriterShmem->num_backend_writes++;

	/*
	 * If the background writer isn't running or the request queue is full,
	 * the backend will have to perform its own fsync request.  But before
	 * forcing that to happen, we can try to compact the background writer
	 * request queue.
	 */
	if (BgWriterShmem->bgwriter_pid == 0 ||
		(BgWriterShmem->num_requests >= BgWriterShmem->max_requests
		&& !CompactBgwriterRequestQueue()))
	{
		LWLockRelease(BgWriterCommLock);
		return false;
	}
	request = &BgWriterShmem->requests[BgWriterShmem->num_requests++];
	request->rnode = rnode;
	request->segno = segno;
	LWLockRelease(BgWriterCommLock);
	return true;
}

/*
 * CompactBgwriterRequestQueue
 *		Remove duplicates from the request queue to avoid backend fsyncs.
 *		Returns "true" if any entries were removed.
 *
 * Although a full fsync request queue is not common, it can lead to severe
 * performance problems when it does happen.  So far, this situation has
 * only been observed to occur when the system is under heavy write load,
 * and especially during the "sync" phase of a checkpoint.	Without this
 * logic, each backend begins doing an fsync for every block written, which
 * gets very expensive and can slow down the whole system.
 *
 * Trying to do this every time the queue is full could lose if there
 * aren't any removable entries.  But that should be vanishingly rare in
 * practice: there's one queue entry per shared buffer.
 */
bool
CompactBgwriterRequestQueue(void)
{
	struct BgWriterSlotMapping
	{
		BgWriterRequest request;
		int			slot;
	};

	int			n,
				preserve_count;
	int			num_skipped = 0;
	HASHCTL		ctl;
	HTAB	   *htab;
	bool	   *skip_slot;

	/* must hold BgWriterCommLock in exclusive mode */
	Assert(LWLockHeldByMe(BgWriterCommLock));

	/* Initialize skip_slot array */
	skip_slot = palloc0(sizeof(bool) * BgWriterShmem->num_requests);

	/* Initialize temporary hash table */
	MemSet(&ctl, 0, sizeof(ctl));
	ctl.keysize = sizeof(BgWriterRequest);
	ctl.entrysize = sizeof(struct BgWriterSlotMapping);
	ctl.hash = tag_hash;
	ctl.hcxt = CurrentMemoryContext;

	htab = hash_create("CompactBgwriterRequestQueue",
					   BgWriterShmem->num_requests,
					   &ctl,
					   HASH_ELEM | HASH_FUNCTION | HASH_CONTEXT);

	/*
	 * The basic idea here is that a request can be skipped if it's followed
	 * by a later, identical request.  It might seem more sensible to work
	 * backwards from the end of the queue and check whether a request is
	 * *preceded* by an earlier, identical request, in the hopes of doing less
	 * copying.  But that might change the semantics, if there's an
	 * intervening FORGET_RELATION_FSYNC or FORGET_DATABASE_FSYNC request, so
	 * we do it this way.  It would be possible to be even smarter if we made
	 * the code below understand the specific semantics of such requests (it
	 * could blow away preceding entries that would end up being canceled
	 * anyhow), but it's not clear that the extra complexity would buy us
	 * anything.
	 */
	for (n = 0; n < BgWriterShmem->num_requests; n++)
	{
		BgWriterRequest *request;
		struct BgWriterSlotMapping *slotmap;
		bool		found;

		/*
		 * We use the request struct directly as a hashtable key.  This
		 * assumes that any padding bytes in the structs are consistently the
		 * same, which should be okay because we zeroed them in
		 * BgWriterShmemInit.  Note also that RelFileNode had better
		 * contain no pad bytes.
		 */
		request = &BgWriterShmem->requests[n];
		slotmap = hash_search(htab, request, HASH_ENTER, &found);
		if (found)
		{
			/* Duplicate, so mark the previous occurrence as skippable */
			skip_slot[slotmap->slot] = true;
			num_skipped++;
		}
		/* Remember slot containing latest occurrence of this request value */
		slotmap->slot = n;
	}

	/* Done with the hash table. */
	hash_destroy(htab);

	/* If no duplicates, we're out of luck. */
	if (!num_skipped)
	{
		pfree(skip_slot);
		return false;
	}

	/* We found some duplicates; remove them. */
	preserve_count = 0;
	for (n = 0; n < BgWriterShmem->num_requests; n++)
	{
		if (skip_slot[n])
			continue;
		BgWriterShmem->requests[preserve_count++] = BgWriterShmem->requests[n];
	}
	ereport(DEBUG1,
	   (errmsg("compacted fsync request queue from %d entries to %d entries",
			   BgWriterShmem->num_requests, preserve_count)));
	BgWriterShmem->num_requests = preserve_count;

	/* Cleanup. */
	pfree(skip_slot);
	return true;
}

/*
 * AbsorbFsyncRequests
 *		Retrieve queued fsync requests and pass them to local smgr.
 *
 * This is exported because it must be called during CreateCheckPoint;
 * we have to be sure we have accepted all pending requests just before
 * we start fsync'ing.  Since CreateCheckPoint sometimes runs in
 * non-bgwriter processes, do nothing if not bgwriter.
 */
void
AbsorbFsyncRequests(void)
{
	BgWriterRequest *requests = NULL;
	BgWriterRequest *request;
	int			n;

	if (!am_bg_writer)
		return;

	/*
	 * We have to PANIC if we fail to absorb all the pending requests (eg,
	 * because our hashtable runs out of memory).  This is because the system
	 * cannot run safely if we are unable to fsync what we have been told to
	 * fsync.  Fortunately, the hashtable is so small that the problem is
	 * quite unlikely to arise in practice.
	 */
	START_CRIT_SECTION();

	/*
	 * We try to avoid holding the lock for a long time by copying the request
	 * array.
	 */
	LWLockAcquire(BgWriterCommLock, LW_EXCLUSIVE);

	/* Transfer write count into pending pgstats message */
	BgWriterStats.m_buf_written_backend += BgWriterShmem->num_backend_writes;
	BgWriterShmem->num_backend_writes = 0;

	n = BgWriterShmem->num_requests;
	if (n > 0)
	{
		requests = (BgWriterRequest *) palloc(n * sizeof(BgWriterRequest));
		memcpy(requests, BgWriterShmem->requests, n * sizeof(BgWriterRequest));
	}
	BgWriterShmem->num_requests = 0;

	LWLockRelease(BgWriterCommLock);

	for (request = requests; n > 0; request++, n--)
		RememberFsyncRequest(request->rnode, request->segno);

	if (requests)
		pfree(requests);

	END_CRIT_SECTION();
}

bool AmBackgroundWriterProcess()
{
	return am_bg_writer;
}
