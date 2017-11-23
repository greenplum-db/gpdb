/*-------------------------------------------------------------------------
 * bgworker.c
 *		Greenplum Database specific pluggable background workers framework
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc.
 *
 * IDENTIFICATION
 *	  src/backend/postmaster/bgworker.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>

#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "cdb/cdbvars.h"
#include "nodes/pg_list.h"
#include "postmaster/postmaster.h"
#include "postmaster/fork_process.h"
#include "postmaster/bgworker.h"
#include "storage/pmsignal.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "tcop/tcopprot.h"
#include "utils/timestamp.h"
#include "utils/ps_status.h"

/*
 * The postmaster's list of registered background workers, in private memory.
 */
List				*BackgroundWorkerList = NIL;
BackgroundWorker	*MyBgworkerEntry = NULL;
bool				StartWorkerNeeded = true;
bool				HaveCrashedWorker = false;

/*
 * Determine how long should we let ServerLoop sleep.
 *
 * In normal conditions we wait at most one minute, to ensure that the other
 * background tasks handled by ServerLoop get done even when no requests are
 * arriving.  However, if there are background workers waiting to be started,
 * we don't actually sleep so that they are quickly serviced.
 */
void
DetermineSleepTime(struct timeval *timeout, int shutdown)
{
	TimestampTz next_wakeup = 0;

	/*
	 * Normal case: either there are no background workers at all, or we're in
	 * a shutdown sequence (during which we ignore bgworkers altogether).
	 */
	if (shutdown > 0 ||
		(!StartWorkerNeeded && !HaveCrashedWorker))
	{
		timeout->tv_sec = 60;
		timeout->tv_usec = 0;
		return;
	}

	if (StartWorkerNeeded)
	{
		timeout->tv_sec = 0;
		timeout->tv_usec = 0;
		return;
	}

	if (HaveCrashedWorker)
	{
		ListCell   *siter;

		/*
		 * When there are crashed bgworkers, we sleep just long enough that
		 * they are restarted when they request to be.	Scan the list to
		 * determine the minimum of all wakeup times according to most recent
		 * crash time and requested restart interval.
		 */
		foreach(siter, BackgroundWorkerList)
		{
			RegisteredBgWorker *rw;
			TimestampTz this_wakeup;

			rw = lfirst(siter);

			if (rw->rw_crashed_at == 0)
				continue;

			if (rw->rw_worker.bgw_restart_time == BGW_NEVER_RESTART)
				continue;

			this_wakeup = TimestampTzPlusMilliseconds(rw->rw_crashed_at,
													  1000L * rw->rw_worker.bgw_restart_time);
			if (next_wakeup == 0 || this_wakeup < next_wakeup)
				next_wakeup = this_wakeup;
		}
	}

	if (next_wakeup != 0)
	{
		int			microsecs;

		TimestampDifference(GetCurrentTimestamp(), next_wakeup,
							&timeout->tv_sec, &microsecs);
		timeout->tv_usec = microsecs;

		/* Ensure we don't exceed one minute */
		if (timeout->tv_sec > 60)
		{
			timeout->tv_sec = 60;
			timeout->tv_usec = 0;
		}
	}
	else
	{
		timeout->tv_sec = 60;
		timeout->tv_usec = 0;
	}
}

static void
bgworker_quickdie(SIGNAL_ARGS)
{
	sigaddset(&BlockSig, SIGQUIT);	/* prevent nested calls */
	PG_SETMASK(&BlockSig);

	/*
	 * * We DO NOT want to run proc_exit() callbacks -- we're here because
	 * shared memory may be corrupted, so we don't want to try to clean up our
	 * transaction.  Just nail the windows shut and get out of town.  Now that
	 * there's an atexit callback to prevent third-party code from breaking
	 * things by calling exit() directly, we have to reset the callbacks
	 * explicitly to make this work as intended.
	 */
	on_exit_reset();

	/*
	 * Note we do exit(0) here, not exit(2) like quickdie.	The reason is that
	 * we don't want to be seen this worker as independently crashed, because
	 * then postmaster would delay restarting it again afterwards. If some
	 * idiot DBA manually sends SIGQUIT to a random bgworker, the "dead man
	 * switch" will ensure that postmaster sees this as a crash.
	 */
	exit(0);
}

/*
 * Standard SIGTERM handler for background workers
 */
static void
bgworker_die(SIGNAL_ARGS)
{
	PG_SETMASK(&BlockSig);

	ereport(FATAL,
			(errcode(ERRCODE_ADMIN_SHUTDOWN),
			 errmsg("terminating background worker \"%s\" due to administrator command",
					MyBgworkerEntry->bgw_name)));
}

static void
do_start_bgworker(void)
{
	sigjmp_buf	local_sigjmp_buf;
	char		buf[MAXPGPATH];
	BackgroundWorker *worker = MyBgworkerEntry;

	if (worker == NULL)
		elog(FATAL, "unable to find bgworker entry");

	/* we are a postmaster subprocess now */
	IsUnderPostmaster = true;
	IsBackgroundWorker = true;

	/* reset MyProcPid */
	MyProcPid = getpid();

	/* record Start Time for logging */
	MyStartTime = time(NULL);

	/* Identify myself via ps */
	snprintf(buf, MAXPGPATH, "bgworker: %s", worker->bgw_name);
	init_ps_display(buf, "", "", "");

	SetProcessingMode(InitProcessing);

	/* Apply PostAuthDelay */
	if (PostAuthDelay > 0)
		pg_usleep(PostAuthDelay * 1000000L);

	/*
	 * If possible, make this process a group leader, so that the postmaster
	 * can signal any child processes too.
	 */
#ifdef HAVE_SETSID
	if (setsid() < 0)
		elog(FATAL, "setsid() failed: %m");
#endif

	/*
	 * Set up signal handlers.
	 *
	 * Bgworker does not support connection now
	 */
	pqsignal(SIGINT, SIG_IGN);
	pqsignal(SIGUSR1, SIG_IGN);
	pqsignal(SIGFPE, SIG_IGN);

	/* SIGTERM and SIGHUP are configurable */
	if (worker->bgw_sigterm)
		pqsignal(SIGTERM, worker->bgw_sigterm);
	else
		pqsignal(SIGTERM, bgworker_die);

	if (worker->bgw_sighup)
		pqsignal(SIGHUP, worker->bgw_sighup);
	else
		pqsignal(SIGHUP, SIG_IGN);

	pqsignal(SIGQUIT, bgworker_quickdie);
	pqsignal(SIGALRM, SIG_IGN);
	pqsignal(SIGPIPE, SIG_IGN);
	pqsignal(SIGUSR2, SIG_IGN);
	pqsignal(SIGCHLD, SIG_DFL);

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
		 * Do we need more cleanup here?  For shmem-connected bgworkers, we
		 * will call InitProcess below, which will install ProcKill as exit
		 * callback.  That will take care of releasing locks, etc.
		 */

		/* and go away */
		proc_exit(1);
	}

	/* We can now handle ereport(ERROR) */
	PG_exception_stack = &local_sigjmp_buf;

	/* Early initialization */
	BaseInit();

	/*		
	 * If necessary, create a per-backend PGPROC struct in shared memory,		
	 * except in the EXEC_BACKEND case where this was done in		
	 * SubPostmasterMain. We must do this before we can use LWLocks (and in		
	 * the EXEC_BACKEND case we already had to do some stuff with LWLocks).		
	 */		
#ifndef EXEC_BACKEND		
	if (worker->bgw_flags & BGWORKER_SHMEM_ACCESS)		
		InitProcess();		
#endif

	/*
	 * Now invoke the user-defined worker code
	 */
	worker->bgw_main(worker->bgw_main_arg);

	/* ... and if it returns, we're done */
	proc_exit(0);
}

/*
 * Start a new bgworker.
 * Starting time conditions must have been checked already.
 *
 * This code is heavily based on autovacuum.c, q.v.
 */
static void
start_bgworker(RegisteredBgWorker *rw)
{
	pid_t		worker_pid;

	ereport(LOG,
			(errmsg("starting background worker process \"%s\"",
					rw->rw_worker.bgw_name)));

#ifndef EXEC_BACKEND
	switch ((worker_pid = fork_process()))
	{
		case -1:
			ereport(LOG,
					(errmsg("could not fork worker process: %m")));
			return;
		case 0:
			/* in postmaster child ... */
			/* Close the postmaster's sockets */
			ClosePostmasterPorts(false);
			/* Lose the postmaster's on-exit routines */
			on_exit_reset();

			/* Do NOT release postmaster's working memory context */
			MyBgworkerEntry = &rw->rw_worker;

			do_start_bgworker();
			break;
		default:
			rw->rw_pid = worker_pid;
	}
#endif
}


/*
 * If the time is right, start one background worker.
 *
 * As a side effect, the bgworker control variables are set or reset whenever
 * there are more workers to start after this one, and whenever the overall
 * system state requires it.
 */
void
StartOneBackgroundWorker(bool fatalError, int pmState, int targetState)
{
	ListCell   *iter;
	TimestampTz now = 0;

	if (fatalError)
	{
		StartWorkerNeeded = false;
		HaveCrashedWorker = false;
		return;					/* not yet */
	}

	HaveCrashedWorker = false;

	foreach(iter, BackgroundWorkerList)
	{
		RegisteredBgWorker *rw;

		rw = lfirst(iter);

		/* already running? */
		if (rw->rw_pid != 0)
			continue;

		/*
		 * I restarted (unless on registration it specified it doesn't want to
		 * be restarted at all).  Check how long ago did a crash last happen.
		 * If the last crash is too recent, don't start it right away; let it
		 * be restarted once enough time has passed.
		 */
		if (rw->rw_crashed_at != 0)
		{
			if (rw->rw_worker.bgw_restart_time == BGW_NEVER_RESTART)
				continue;
			if (now == 0)
				now = GetCurrentTimestamp();

			if (!TimestampDifferenceExceeds(rw->rw_crashed_at, now,
											rw->rw_worker.bgw_restart_time * 1000))
			{
				HaveCrashedWorker = true;
				continue;
			}
		}
		if (pmState == targetState)
		{
			/* reset crash time before calling assign_backendlist_entry */
			rw->rw_crashed_at = 0;

			/*
			 * If necessary, allocate and assign the Backend element.  Note we
			 * must do this before forking, so that we can handle out of
			 * memory properly. If not connected, we don't need a Backend
			 * element, but we still need a PMChildSlot.
			 */
			rw->rw_child_slot = MyPMChildSlot = AssignPostmasterChildSlot();
			start_bgworker(rw); /* sets rw->rw_pid */

			/*
			 * Have ServerLoop call us again.  Note that there might not
			 * actually *be* another runnable worker, but we don't care all
			 * that much; we will find out the next time we run.
			 */
			StartWorkerNeeded = true;
			return;
		}
	}
	/* no runnable worker found */
	StartWorkerNeeded = false;
}

/*
 * Send a signal to bgworkers
 */
bool
SignalUnconnectedWorkers(int signal, void (*signal_child) (pid_t, int))
{
	ListCell   *iter;
	bool		signaled = false;

	foreach(iter, BackgroundWorkerList)
	{
		RegisteredBgWorker *rw;

		rw = lfirst(iter);

		if (rw->rw_pid == 0)
			continue;
		ereport(DEBUG4,
				(errmsg_internal("sending signal %d to process %d",
								 signal, (int) rw->rw_pid)));
		(*signal_child) (rw->rw_pid, signal);
		signaled = true;
	}
	return signaled;
}

/*
 * Scan the bgworkers list and see if the given PID (which has just stopped
 * or crashed) is in it.  Handle its shutdown if so, and return true.  If not a
 * bgworker, return false.
 *
 * This is heavily based on CleanupBackend.  One important difference is that
 * we don't know yet that the dying process is a bgworker, so we must be silent
 * until we're sure it is.
 */
bool
CleanupBackgroundWorker(int pid, int exitstatus, void (*handleChildCrash) (int, int, const char *),
						void(*logChildExit) (int, const char *, int, int))
{
	char		namebuf[MAXPGPATH];
	ListCell   *iter;

	foreach(iter, BackgroundWorkerList)
	{
		RegisteredBgWorker *rw;

		rw = lfirst(iter);

		if (rw->rw_pid != pid)
			continue;

		snprintf(namebuf, MAXPGPATH, "%s: %s", _("worker process"),
				 rw->rw_worker.bgw_name);

		/* Delay restarting any bgworker that exits with a nonzero status. */
		if (!((exitstatus) == 0))
			rw->rw_crashed_at = GetCurrentTimestamp();
		else
			rw->rw_crashed_at = 0;

		/*
		 * Additionally, for shared-memory-connected workers, just like a
		 * backend, any exit status other than 0 or 1 is considered a crash
		 * and causes a system-wide restart.
		 */
		if (rw->rw_worker.bgw_flags & BGWORKER_SHMEM_ACCESS)
		{
			if (!((exitstatus) == 0) && !(WIFEXITED(exitstatus) && WEXITSTATUS(exitstatus) == 1))
			{
				rw->rw_crashed_at = GetCurrentTimestamp();
				(*handleChildCrash) (pid, exitstatus, namebuf);
				return true;
			}
		}
		if (!ReleasePostmasterChildSlot(rw->rw_child_slot))
		{
			/*
			 * Uh-oh, the child failed to clean itself up.	Treat as a crash
			 * after all.
			 */
			rw->rw_crashed_at = GetCurrentTimestamp();
			(*handleChildCrash) (pid, exitstatus, namebuf);
			return true;
		}
		rw->rw_pid = 0;
		rw->rw_child_slot = 0;
		(*logChildExit) (LOG, namebuf, pid, exitstatus);
		return true;
	}

	return false;
}

/*
 * Count up number of worker processes that did not request backend connections
 */
int
CountUnconnectedWorkers(void)
{
	ListCell   *iter;
	int			cnt = 0;

	foreach(iter, BackgroundWorkerList)
	{
		RegisteredBgWorker *rw;

		rw = lfirst(iter);

		if (rw->rw_pid == 0)
			continue;

		cnt++;
	}
	return cnt;
}

/*
 * Register a new background worker.
 *
 * This can only be called in the _PG_init function of a module library
 * that's loaded by shared_preload_libraries; otherwise it has no effect.
 */
void
RegisterBackgroundWorker(BackgroundWorker *worker)
{
	RegisteredBgWorker *rw;
	int			namelen = strlen(worker->bgw_name);

	if (!IsUnderPostmaster)
		ereport(LOG,
				(errmsg("registering background worker: %s", worker->bgw_name)));

	if (Gp_role == GP_ROLE_UTILITY)
		return;

	if (!process_shared_preload_libraries_in_progress)
	{
		if (!IsUnderPostmaster)
			ereport(LOG,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("background worker \"%s\": must be registered in shared_preload_libraries",
							worker->bgw_name)));
		return;
	}

	if ((worker->bgw_restart_time < 0 &&
		 worker->bgw_restart_time != BGW_NEVER_RESTART) ||
		(worker->bgw_restart_time > USECS_PER_DAY / 1000))
	{
		if (!IsUnderPostmaster)
			ereport(LOG,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("background worker \"%s\": invalid restart interval",
							worker->bgw_name)));
		return;
	}

	/*
	 * Copy the registration data into the registered workers list.
	 */
	rw = malloc(sizeof(RegisteredBgWorker) + namelen + 1);
	if (rw == NULL)
	{
		ereport(LOG,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("out of memory")));
		return;
	}
	rw->rw_worker = *worker;
	rw->rw_worker.bgw_name = ((char *) rw) + sizeof(RegisteredBgWorker);
	strlcpy(rw->rw_worker.bgw_name, worker->bgw_name, namelen + 1);
	rw->rw_pid = 0;
	rw->rw_child_slot = 0;
	rw->rw_crashed_at = 0;

	BackgroundWorkerList = lappend(BackgroundWorkerList, rw);
}

/*
 * Return the number of background workers registered that have at least
 * one of the passed flag bits set.
 */
static int
GetNumRegisteredBackgroundWorkers(int flags)
{
	ListCell   *iter;
	int			count = 0;

	foreach(iter, BackgroundWorkerList)
	{
		RegisteredBgWorker *rw;

		rw = lfirst(iter);

		if (flags != 0 &&
			!(rw->rw_worker.bgw_flags & flags))
			continue;

		count++;
	}

	return count;
}

/*
 * Return the number of bgworkers that need to have PGPROC entries.
 */
int
GetNumShmemAttachedBgworkers(void)
{
	return GetNumRegisteredBackgroundWorkers(BGWORKER_SHMEM_ACCESS);
}

void
BackgroundWorkerUnblockSignals(void)
{
	PG_SETMASK(&UnBlockSig);
}

void
HandleBgworkerCrash(int pid, void (*signal_child) (pid_t, int), bool fatalError, int sendStop)
{
	ListCell   *cell;

	/* Process background workers. */
	foreach(cell, BackgroundWorkerList)
	{
		RegisteredBgWorker *rw;

		rw = lfirst(cell);
		if (rw->rw_pid == 0)
			continue;		/* not running */
		if (rw->rw_pid == pid)
		{
			/*
			 * Found entry for freshly-dead worker, so remove it.
			 */
			(void) ReleasePostmasterChildSlot(rw->rw_child_slot);
			rw->rw_pid = 0;
			rw->rw_child_slot = 0;
			/* don't reset crashed_at */
			/* Keep looping so we can signal remaining workers */
		}
		else
		{
			/*
			 * This worker is still alive.	Unless we did so already, tell it
			 * to commit hara-kiri.
			 *
			 * SIGQUIT is the special signal that says exit without proc_exit
			 * and let the user know what's going on. But if SendStop is set
			 * (-s on command line), then we send SIGSTOP instead, so that we
			 * can get core dumps from all backends by hand.
			 */
			if (!fatalError)
			{
				ereport(DEBUG2,
							(errmsg_internal("sending %s to process %d",
											 (sendStop ? "SIGSTOP" : "SIGQUIT"),
											 (int) rw->rw_pid)));
				(*signal_child)(rw->rw_pid, (sendStop ? SIGSTOP : SIGQUIT));
			}
		}
	}
}
