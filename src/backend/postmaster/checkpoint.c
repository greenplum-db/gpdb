/*-------------------------------------------------------------------------
 *
 * checkpoint.c
 *
 * The checkpoint server process handles periodically doing checkpoints and regular checkpoint
 * requests.  Previously, the background writer use to do this duty.  But we ended up with
 * a deadlock with the new FileRep functionality, so this functionality was split out into its
 * own server.
 *
 * It will automatically dispatch a checkpoint after a certain amount of time has
 * elapsed since the last one, and it can be signaled to perform requested
 * checkpoints as well.  (The GUC parameter that mandates a checkpoint every
 * so many WAL segments is implemented by having backends signal the checkpoint server
 * when they fill WAL segments; the checkpoint server itself doesn't watch for the
 * condition.)
 *
 * The checkpoint server is started by the postmaster as soon as the startup subprocess
 * finishes.  It remains alive until the postmaster commands it to terminate.
 * Normal termination is by SIGUSR2, which instructs the bgwriter to execute
 * a shutdown checkpoint and then exit(0).	(All backends must be stopped
 * before SIGUSR2 is issued!)  Emergency termination is by SIGQUIT; like any
 * backend, the checkpoint server will simply abort and exit on SIGQUIT.
 *
 * If the checkpoint server exits unexpectedly, the postmaster treats that the same
 * as a backend crash: shared memory may be corrupted, so remaining backends
 * should be killed by SIGQUIT and then a recovery cycle started.  (Even if
 * shared memory isn't corrupted, we have lost information about which
 * files need to be fsync'd for the next checkpoint, and so a system
 * restart needs to be forced.)
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <signal.h>
#include <time.h>
#include <unistd.h>

#include "access/xlog_internal.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "postmaster/bgwriter.h"
#include "storage/fd.h"
#include "storage/freespace.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/pmsignal.h"
#include "storage/shmem.h"
#include "storage/smgr.h"
#include "tcop/tcopprot.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/resowner.h"
#include "postmaster/fork_process.h"
#include "postmaster/postmaster.h"

#include "tcop/tcopprot.h" /* quickdie() */

/*----------
 * Shared memory area for communication between checkpoint server and backends
 *
 * The ckpt counters allow backends to watch for completion of a checkpoint
 * request they send.  Here's how it works:
 *	* At start of a checkpoint, bgwriter increments ckpt_started.
 *	* On completion of a checkpoint, bgwriter sets ckpt_done to
 *	  equal ckpt_started.
 *	* On failure of a checkpoint, bgwrite first increments ckpt_failed,
 *	  then sets ckpt_done to equal ckpt_started.
 * All three fields are declared sig_atomic_t to ensure they can be read
 * and written without explicit locking.  The algorithm for backends is:
 *	1. Record current values of ckpt_failed and ckpt_started (in that
 *	   order!).
 *	2. Send signal to request checkpoint.
 *	3. Sleep until ckpt_started changes.  Now you know a checkpoint has
 *	   begun since you started this algorithm (although *not* that it was
 *	   specifically initiated by your signal).
 *	4. Record new value of ckpt_started.
 *	5. Sleep until ckpt_done >= saved value of ckpt_started.  (Use modulo
 *	   arithmetic here in case counters wrap around.)  Now you know a
 *	   checkpoint has started and completed, but not whether it was
 *	   successful.
 *	6. If ckpt_failed is different from the originally saved value,
 *	   assume request failed; otherwise it was definitely successful.
 *
 * An additional field is ckpt_time_warn; this is also sig_atomic_t for
 * simplicity, but is only used as a boolean.  If a backend is requesting
 * a checkpoint for which a checkpoints-too-close-together warning is
 * reasonable, it should set this field TRUE just before sending the signal.
 *
 *----------
 */

typedef struct
{
	pid_t		checkpoint_server_pid;	/* PID of checkpoint server (0 if not started) */

	sig_atomic_t ckpt_started;	/* advances when checkpoint starts */
	sig_atomic_t ckpt_done;		/* advances when checkpoint done */
	sig_atomic_t ckpt_failed;	/* advances when checkpoint fails */

	sig_atomic_t ckpt_time_warn;	/* warn if too soon since last ckpt? */
} CheckpointShmemStruct;

static CheckpointShmemStruct *CheckpointShmem;

/*
 * GUC parameters
 */
int			CheckPointTimeout = 300;
int			CheckPointWarning = 30;

/*
 * Flags set by interrupt handlers for later service in the main loop.
 */
static volatile sig_atomic_t got_SIGHUP = false;
static volatile sig_atomic_t checkpoint_requested = false;
static volatile sig_atomic_t shutdown_requested = false;

/*
 * Private state
 */
static bool am_checkpoint_server = false;

static bool ckpt_active = false;

static time_t last_checkpoint_time;

static void CheckpointSigHupHandler(SIGNAL_ARGS);
static void ReqCheckpointHandler(SIGNAL_ARGS);
static void ReqShutdownHandler(SIGNAL_ARGS);



/*
 * Main entry point for bgwriter process
 *
 * This is invoked from AuxiliaryProcessMain, which has already created the basic
 * execution environment, but not enabled signals yet.
 */
void
CheckpointMain(void)
{
	sigjmp_buf	local_sigjmp_buf;
	MemoryContext checkpoint_context;

	Assert(CheckpointShmem != NULL);
	CheckpointShmem->checkpoint_server_pid = MyProcPid;
	am_checkpoint_server = true;

	/*
	 * If possible, make this process a group leader, so that the postmaster
	 * can signal any child processes too.  (bgwriter probably never has
	 * any child processes, but for consistency we make all postmaster
	 * child processes do this.)
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
	 * process to participate in sinval messaging.
	 */
	pqsignal(SIGHUP, CheckpointSigHupHandler);	/* set flag to read config file */
	pqsignal(SIGINT, ReqCheckpointHandler);		/* request checkpoint */
	pqsignal(SIGTERM, SIG_IGN); /* ignore SIGTERM */
	pqsignal(SIGQUIT, quickdie);		/* hard crash time: nothing bg-writer specific, just use the standard */
	pqsignal(SIGALRM, SIG_IGN);
	pqsignal(SIGPIPE, SIG_IGN);
	pqsignal(SIGUSR1, SIG_IGN); /* reserve for sinval */
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
	last_checkpoint_time = time(NULL);

	/*
	 * Create a resource owner to keep track of our resources (currently only
	 * buffer pins).
	 */
	CurrentResourceOwner = ResourceOwnerCreate(NULL, "Checkpoint Server");

	/*
	 * Create a memory context that we will do all our work in.  We do this so
	 * that we can reset the context during error recovery and thereby avoid
	 * possible memory leaks.  Formerly this code just ran in
	 * TopMemoryContext, but resetting that would be a really bad idea.
	 */
	checkpoint_context = AllocSetContextCreate(TopMemoryContext,
											 "Background Writer",
											 ALLOCSET_DEFAULT_MINSIZE,
											 ALLOCSET_DEFAULT_INITSIZE,
											 ALLOCSET_DEFAULT_MAXSIZE);
	MemoryContextSwitchTo(checkpoint_context);

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

		/* Warn any waiting backends that the checkpoint failed. */
		if (ckpt_active)
		{
			/* use volatile pointer to prevent code rearrangement */
			volatile CheckpointShmemStruct *css = CheckpointShmem;

			css->ckpt_failed++;
			css->ckpt_done = css->ckpt_started;
			ckpt_active = false;
		}

		/*
		 * Now return to normal top-level context and clear ErrorContext for
		 * next time.
		 */
		MemoryContextSwitchTo(checkpoint_context);
		FlushErrorState();

		/* Flush any leaked data in the top-level context */
		MemoryContextResetAndDeleteChildren(checkpoint_context);

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
		bool		do_checkpoint = false;
		bool		force_checkpoint = false;
		time_t		now;
		int			elapsed_secs;
		long		udelay;

		udelay = 1000000L;	/* One second */

		/*
		 * Emergency bailout if postmaster has died.  This is to avoid the
		 * necessity for manual cleanup of all postmaster children.
		 */
		if (!PostmasterIsAlive(true))
			exit(1);

		if (got_SIGHUP)
		{
			got_SIGHUP = false;
			ProcessConfigFile(PGC_SIGHUP);
		}
		if (checkpoint_requested)
		{
			checkpoint_requested = false;
			do_checkpoint = true;
			force_checkpoint = true;
		}
		if (shutdown_requested)
		{
			/*
			 * From here on, elog(ERROR) should end with exit(1), not send
			 * control back to the sigsetjmp block above
			 */
			ExitOnAnyError = true;

			/* Normal exit from the checkpoint server is here */
			if (Debug_print_server_processes ||
				Debug_print_qd_mirroring ||
				log_min_messages <= DEBUG5)
			{
				ereport((log_min_messages > DEBUG5 ? LOG : DEBUG5),
						(errmsg("normal exit from checkpoint server")));
			}
			proc_exit(0);		/* done */
		}

		/*
		 * Do an unforced checkpoint if too much time has elapsed since the
		 * last one.
		 */
		now = time(NULL);
		elapsed_secs = now - last_checkpoint_time;
		if (elapsed_secs >= CheckPointTimeout)
			do_checkpoint = true;

		/*
		 * Do a checkpoint if requested, otherwise do one cycle of
		 * dirty-buffer writing.
		 */
		if (do_checkpoint)
		{
			/*
			 * We will warn if (a) too soon since last checkpoint (whatever
			 * caused it) and (b) somebody has set the ckpt_time_warn flag
			 * since the last checkpoint start.  Note in particular that this
			 * implementation will not generate warnings caused by
			 * CheckPointTimeout < CheckPointWarning.
			 */
			if (CheckpointShmem->ckpt_time_warn &&
				elapsed_secs < CheckPointWarning)
				ereport(LOG,
						(errmsg("checkpoints are occurring too frequently (%d seconds apart)",
								elapsed_secs),
						 errhint("Consider increasing the configuration parameter \"checkpoint_segments\".")));
			CheckpointShmem->ckpt_time_warn = false;

			/*
			 * Indicate checkpoint start to any waiting backends.
			 */
			ckpt_active = true;
			CheckpointShmem->ckpt_started++;

			CreateCheckPoint(false, force_checkpoint);

			/*
			 * After any checkpoint, close all smgr files.	This is so we
			 * won't hang onto smgr references to deleted files indefinitely.
			 */
			smgrcloseall();

			/*
			 * Signal the background writer to also call smgrcloseall.
			 */
			RequestCheckpointSmgrCloseAll();

			/*
			 * Indicate checkpoint completion to any waiting backends.
			 */
			CheckpointShmem->ckpt_done = CheckpointShmem->ckpt_started;
			ckpt_active = false;

			/*
			 * Note we record the checkpoint start time not end time as
			 * last_checkpoint_time.  This is so that time-driven checkpoints
			 * happen at a predictable spacing.
			 */
			last_checkpoint_time = now;
		}

		if (!(got_SIGHUP || checkpoint_requested || shutdown_requested))
			pg_usleep(udelay);
	}
}


/* --------------------------------
 *		signal handler routines
 * --------------------------------
 */

/* SIGHUP: set flag to re-read config file at next convenient time */
static void
CheckpointSigHupHandler(SIGNAL_ARGS)
{
	got_SIGHUP = true;
}

/* SIGINT: set flag to run a normal checkpoint right away */
static void
ReqCheckpointHandler(SIGNAL_ARGS)
{
	checkpoint_requested = true;
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
 * CheckpointShmemSize
 *		Compute space needed for checkpoint server related shared memory
 */
Size
CheckpointShmemSize(void)
{
	Size		size;

	size = sizeof(CheckpointShmemStruct);

	return size;
}

/*
 * CheckpointShmemInit
 *		Allocate and initialize checkpoint server related shared memory
 */
void
CheckpointShmemInit(void)
{
	bool		found;

	CheckpointShmem = (CheckpointShmemStruct *)
		ShmemInitStruct("Checkpoint Server Data",
						CheckpointShmemSize(),
						&found);
	if (CheckpointShmem == NULL)
		ereport(FATAL,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("not enough shared memory for checkpoint server")));
	if (found)
		return;					/* already initialized */

	MemSet(CheckpointShmem, 0, sizeof(CheckpointShmemStruct));
}

/*
 * RequestCheckpoint
 *		Called in backend processes to request an immediate checkpoint
 *
 * If waitforit is true, wait until the checkpoint is completed
 * before returning; otherwise, just signal the request and return
 * immediately.
 *
 * If warnontime is true, and it's "too soon" since the last checkpoint,
 * the bgwriter will log a warning.  This should be true only for checkpoints
 * caused due to xlog filling, else the warning will be misleading.
 */
void
RequestCheckpoint(bool waitforit, bool warnontime)
{
	/* use volatile pointer to prevent code rearrangement */
	volatile CheckpointShmemStruct *css = CheckpointShmem;
	sig_atomic_t old_failed = css->ckpt_failed;
	sig_atomic_t old_started = css->ckpt_started;

	/*
	 * If in a standalone backend, just do it ourselves.
	 */
	if (!IsPostmasterEnvironment)
	{
		CreateCheckPoint(false, true);

		/*
		 * After any checkpoint, close all smgr files.	This is so we won't
		 * hang onto smgr references to deleted files indefinitely.
		 */
		smgrcloseall();

		return;
	}

	/* Set warning request flag if appropriate */
	if (warnontime)
		css->ckpt_time_warn = true;

	/*
	 * Send signal to request checkpoint.  When waitforit is false, we
	 * consider failure to send the signal to be nonfatal.
	 */
	if (CheckpointShmem->checkpoint_server_pid == 0)
		elog(waitforit ? ERROR : LOG,
			 "could not request checkpoint because checkpoint server not running");
	if (kill(CheckpointShmem->checkpoint_server_pid, SIGINT) != 0)
		elog(waitforit ? ERROR : LOG,
			 "could not signal for checkpoint: %m");

	/*
	 * If requested, wait for completion.  We detect completion according to
	 * the algorithm given above.
	 */
	if (waitforit)
	{
		while (css->ckpt_started == old_started)
		{
			CHECK_FOR_INTERRUPTS();
			pg_usleep(100000L);
		}
		old_started = css->ckpt_started;

		/*
		 * We are waiting for ckpt_done >= old_started, in a modulo sense.
		 * This is a little tricky since we don't know the width or signedness
		 * of sig_atomic_t.  We make the lowest common denominator assumption
		 * that it is only as wide as "char".  This means that this algorithm
		 * will cope correctly as long as we don't sleep for more than 127
		 * completed checkpoints.  (If we do, we will get another chance to
		 * exit after 128 more checkpoints...)
		 */
		while (((signed char) (css->ckpt_done - old_started)) < 0)
		{
			CHECK_FOR_INTERRUPTS();
			pg_usleep(100000L);
		}
		if (css->ckpt_failed != old_failed)
			ereport(ERROR,
					(errmsg("checkpoint request failed"),
					 errhint("Consult recent messages in the server log for details.")));
	}
}
