/*-------------------------------------------------------------------------
 *
 * autovacuum.c
 *
 * PostgreSQL Integrated Autovacuum Daemon
 *
 * The autovacuum system is structured in two different kinds of processes: the
 * autovacuum launcher and the autovacuum worker.  The launcher is an
 * always-running process, started by the postmaster when the autovacuum GUC
 * parameter is set.  The launcher schedules autovacuum workers to be started
 * when appropriate.  The workers are the processes which execute the actual
 * vacuuming; they connect to a database as determined in the launcher, and
 * once connected they examine the catalogs to select the tables to vacuum.
 *
 * The autovacuum launcher cannot start the worker processes by itself,
 * because doing so would cause robustness issues (namely, failure to shut
 * them down on exceptional conditions, and also, since the launcher is
 * connected to shared memory and is thus subject to corruption there, it is
 * not as robust as the postmaster).  So it leaves that task to the postmaster.
 *
 * There is an autovacuum shared memory area, where the launcher stores
 * information about the database it wants vacuumed.  When it wants a new
 * worker to start, it sets a flag in shared memory and sends a signal to the
 * postmaster.	Then postmaster knows nothing more than it must start a worker;
 * so it forks a new child, which turns into a worker.	This new process
 * connects to shared memory, and there it can inspect the information that the
 * launcher has set up.
 *
 * If the fork() call fails in the postmaster, it sets a flag in the shared
 * memory area, and sends a signal to the launcher.  The launcher, upon
 * noticing the flag, can try starting the worker again by resending the
 * signal.	Note that the failure can only be transient (fork failure due to
 * high load, memory pressure, too many processes, etc); more permanent
 * problems, like failure to connect to a database, are detected later in the
 * worker and dealt with just by having the worker exit normally.  The launcher
 * will launch a new worker again later, per schedule.
 *
 * When the worker is done vacuuming it sends SIGUSR2 to the launcher.	The
 * launcher then wakes up and is able to launch another worker, if the schedule
 * is so tight that a new worker is needed immediately.  At this time the
 * launcher can also balance the settings for the various remaining workers'
 * cost-based vacuum delay feature.
 *
 * Note that there can be more than one worker in a database concurrently.
 * They will store the table they are currently vacuuming in shared memory, so
 * that other workers avoid being blocked waiting for the vacuum lock for that
 * table.  They will also reload the pgstats data just before vacuuming each
 * table, to avoid vacuuming a table that was just finished being vacuumed by
 * another worker and thus is no longer noted in shared memory.  However,
 * there is a window (caused by pgstat delay) on which a worker may choose a
 * table that was already vacuumed; this is a bug in the current design.
 *
 * Portions Copyright (c) 1996-2013, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/postmaster/autovacuum.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/multixact.h"
#include "access/reloptions.h"
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/dependency.h"
#include "catalog/namespace.h"
#include "catalog/pg_database.h"
#include "commands/dbcommands.h"
#include "commands/vacuum.h"
#include "lib/ilist.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "postmaster/fork_process.h"
#include "postmaster/postmaster.h"
#include "storage/bufmgr.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/pmsignal.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/procsignal.h"
#include "storage/sinval.h"
#include "storage/sinvaladt.h"
#include "tcop/tcopprot.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/timeout.h"
#include "utils/timestamp.h"
#include "utils/tqual.h"

#include "cdb/cdbvars.h"
#include "utils/faultinjector.h"


/*
 * GUC parameters
 */
bool		autovacuum_start_daemon = false;
int			autovacuum_max_workers;
int			autovacuum_naptime;
int			autovacuum_vac_thresh;
double		autovacuum_vac_scale;
int			autovacuum_anl_thresh;
double		autovacuum_anl_scale;
int			autovacuum_freeze_max_age;

int			autovacuum_vac_cost_delay;
int			autovacuum_vac_cost_limit;

int			Log_autovacuum_min_duration = 0;

/* how long to keep pgstat data in the launcher, in milliseconds */
#define STATS_READ_DELAY 1000

/* the minimum allowed time between two awakenings of the launcher */
#define MIN_AUTOVAC_SLEEPTIME 100.0		/* milliseconds */

/* Flags to tell if we are in an autovacuum process */
static bool am_autovacuum_launcher = false;
static bool am_autovacuum_worker = false;

/* Flags set by signal handlers */
static volatile sig_atomic_t got_SIGHUP = false;
static volatile sig_atomic_t got_SIGUSR2 = false;
static volatile sig_atomic_t got_SIGTERM = false;

/* Comparison points for determining whether freeze_max_age is exceeded */
static TransactionId recentXid;
static MultiXactId recentMulti;

/* Default freeze ages to use for autovacuum (varies by database) */
static int	default_freeze_min_age;
static int	default_freeze_table_age;

/* Memory context for long-lived data */
static MemoryContext AutovacMemCxt;

/* struct to keep track of databases in launcher */
typedef struct avl_dbase
{
	Oid			adl_datid;		/* hash key -- must be first */
	TimestampTz adl_next_worker;
	int			adl_score;
	dlist_node	adl_node;
} avl_dbase;

/* struct to keep track of databases in worker */
typedef struct avw_dbase
{
	Oid			adw_datid;
	char	   *adw_name;
	TransactionId adw_frozenxid;
	MultiXactId adw_frozenmulti;
	PgStat_StatDBEntry *adw_entry;
} avw_dbase;

/* struct to keep track of tables to vacuum and/or analyze, in 1st pass */
typedef struct av_relation
{
	Oid			ar_toastrelid;	/* hash key - must be first */
	Oid			ar_relid;
	bool		ar_hasrelopts;
	AutoVacOpts ar_reloptions;	/* copy of AutoVacOpts from the main table's
								 * reloptions, or NULL if none */
} av_relation;

/* struct to keep track of tables to vacuum and/or analyze, after rechecking */
typedef struct autovac_table
{
	Oid			at_relid;
	bool		at_dovacuum;
	bool		at_doanalyze;
	int			at_freeze_min_age;
	int			at_freeze_table_age;
	int			at_vacuum_cost_delay;
	int			at_vacuum_cost_limit;
	bool		at_wraparound;
	char	   *at_relname;
	char	   *at_nspname;
	char	   *at_datname;
} autovac_table;

/*-------------
 * This struct holds information about a single worker's whereabouts.  We keep
 * an array of these in shared memory, sized according to
 * autovacuum_max_workers.
 *
 * wi_links		entry into free list or running list
 * wi_dboid		OID of the database this worker is supposed to work on
 * wi_tableoid	OID of the table currently being vacuumed, if any
 * wi_proc		pointer to PGPROC of the running worker, NULL if not started
 * wi_launchtime Time at which this worker was launched
 * wi_cost_*	Vacuum cost-based delay parameters current in this worker
 *
 * All fields are protected by AutovacuumLock, except for wi_tableoid which is
 * protected by AutovacuumScheduleLock (which is read-only for everyone except
 * that worker itself).
 *-------------
 */
typedef struct WorkerInfoData
{
	dlist_node	wi_links;
	Oid			wi_dboid;
	Oid			wi_tableoid;
	PGPROC	   *wi_proc;
	TimestampTz wi_launchtime;
	int			wi_cost_delay;
	int			wi_cost_limit;
	int			wi_cost_limit_base;
} WorkerInfoData;

typedef struct WorkerInfoData *WorkerInfo;

/*
 * Possible signals received by the launcher from remote processes.  These are
 * stored atomically in shared memory so that other processes can set them
 * without locking.
 */
typedef enum
{
	AutoVacForkFailed,			/* failed trying to start a worker */
	AutoVacRebalance,			/* rebalance the cost limits */
	AutoVacNumSignals			/* must be last */
}	AutoVacuumSignal;

/*-------------
 * The main autovacuum shmem struct.  On shared memory we store this main
 * struct and the array of WorkerInfo structs.	This struct keeps:
 *
 * av_signal		set by other processes to indicate various conditions
 * av_launcherpid	the PID of the autovacuum launcher
 * av_freeWorkers	the WorkerInfo freelist
 * av_runningWorkers the WorkerInfo non-free queue
 * av_startingWorker pointer to WorkerInfo currently being started (cleared by
 *					the worker itself as soon as it's up and running)
 *
 * This struct is protected by AutovacuumLock, except for av_signal and parts
 * of the worker list (see above).
 *-------------
 */
typedef struct
{
	sig_atomic_t av_signal[AutoVacNumSignals];
	pid_t		av_launcherpid;
	dlist_head	av_freeWorkers;
	dlist_head	av_runningWorkers;
	WorkerInfo	av_startingWorker;
} AutoVacuumShmemStruct;

static AutoVacuumShmemStruct *AutoVacuumShmem;

/*
 * the database list (of avl_dbase elements) in the launcher, and the context
 * that contains it
 */
static dlist_head DatabaseList = DLIST_STATIC_INIT(DatabaseList);
static MemoryContext DatabaseListCxt = NULL;

/* Pointer to my own WorkerInfo, valid on each worker */
static WorkerInfo MyWorkerInfo = NULL;

/* PID of launcher, valid only in worker while shutting down */
int			AutovacuumLauncherPid = 0;

#ifdef EXEC_BACKEND
static pid_t avlauncher_forkexec(void);
static pid_t avworker_forkexec(void);
#endif
NON_EXEC_STATIC void AutoVacWorkerMain(int argc, char *argv[]) __attribute__((noreturn));
NON_EXEC_STATIC void AutoVacLauncherMain(int argc, char *argv[]) __attribute__((noreturn));

static Oid	do_start_worker(void);
static void launcher_determine_sleep(bool canlaunch, bool recursing,
						 struct timeval * nap);
static void launch_worker(TimestampTz now);
static List *get_database_list(void);
static void rebuild_database_list(Oid newdb);
static int	db_comparator(const void *a, const void *b);
static void autovac_balance_cost(void);

static void do_autovacuum(void);
static void FreeWorkerInfo(int code, Datum arg);

static autovac_table *table_recheck_autovac(Oid relid, HTAB *table_toast_map,
					  TupleDesc pg_class_desc);
static void relation_needs_vacanalyze(Oid relid, AutoVacOpts *relopts,
						  Form_pg_class classForm,
						  PgStat_StatTabEntry *tabentry,
						  bool *dovacuum, bool *doanalyze, bool *wraparound);

static void autovacuum_do_vac_analyze(autovac_table *tab,
						  BufferAccessStrategy bstrategy);
static AutoVacOpts *extract_autovac_opts(HeapTuple tup,
					 TupleDesc pg_class_desc);
static PgStat_StatTabEntry *get_pgstat_tabentry_relid(Oid relid, bool isshared,
						  PgStat_StatDBEntry *shared,
						  PgStat_StatDBEntry *dbentry);
static void autovac_report_activity(autovac_table *tab);
static void avl_sighup_handler(SIGNAL_ARGS);
static void avl_sigusr2_handler(SIGNAL_ARGS);
static void avl_sigterm_handler(SIGNAL_ARGS);
static void autovac_refresh_stats(void);



/********************************************************************
 *					  AUTOVACUUM LAUNCHER CODE
 ********************************************************************/

#ifdef EXEC_BACKEND
/*
 * forkexec routine for the autovacuum launcher process.
 *
 * Format up the arglist, then fork and exec.
 */
static pid_t
avlauncher_forkexec(void)
{
	char	   *av[10];
	int			ac = 0;

	av[ac++] = "postgres";
	av[ac++] = "--forkavlauncher";
	av[ac++] = NULL;			/* filled in by postmaster_forkexec */
	av[ac] = NULL;

	Assert(ac < lengthof(av));

	return postmaster_forkexec(ac, av);
}

/*
 * We need this set from the outside, before InitProcess is called
 */
void
AutovacuumLauncherIAm(void)
{
	am_autovacuum_launcher = true;
}
#endif

/*
 * Main entry point for autovacuum launcher process, to be called from the
 * postmaster.
 */
int
StartAutoVacLauncher(void)
{
	pid_t		AutoVacPID;

#ifdef EXEC_BACKEND
	switch ((AutoVacPID = avlauncher_forkexec()))
#else
	switch ((AutoVacPID = fork_process()))
#endif
	{
		case -1:
			ereport(LOG,
				 (errmsg("could not fork autovacuum launcher process: %m")));
			return 0;

#ifndef EXEC_BACKEND
		case 0:
			/* in postmaster child ... */
			/* Close the postmaster's sockets */
			ClosePostmasterPorts(false);

			/* Lose the postmaster's on-exit routines */
			on_exit_reset();

			AutoVacLauncherMain(0, NULL);
			break;
#endif
		default:
			return (int) AutoVacPID;
	}

	/* shouldn't get here */
	return 0;
}

/*
 * Main loop for the autovacuum launcher process.
 */
NON_EXEC_STATIC void
AutoVacLauncherMain(int argc, char *argv[])
{
	sigjmp_buf	local_sigjmp_buf;

	/* we are a postmaster subprocess now */
	IsUnderPostmaster = true;
	am_autovacuum_launcher = true;

	/* reset MyProcPid */
	MyProcPid = getpid();

	/* record Start Time for logging */
	MyStartTime = time(NULL);

	/* Identify myself via ps */
	init_ps_display("autovacuum launcher process", "", "", "");

	ereport(LOG,
			(errmsg("autovacuum launcher started")));

	if (PostAuthDelay)
		pg_usleep(PostAuthDelay * 1000000L);

	SetProcessingMode(InitProcessing);

	/*
	 * If possible, make this process a group leader, so that the postmaster
	 * can signal any child processes too.	(autovacuum probably never has any
	 * child processes, but for consistency we make all postmaster child
	 * processes do this.)
	 */
#ifdef HAVE_SETSID
	if (setsid() < 0)
		elog(FATAL, "setsid() failed: %m");
#endif

	/*
	 * Set up signal handlers.	We operate on databases much like a regular
	 * backend, so we use the same signal handling.  See equivalent code in
	 * tcop/postgres.c.
	 */
	pqsignal(SIGHUP, avl_sighup_handler);
	pqsignal(SIGINT, StatementCancelHandler);
	pqsignal(SIGTERM, avl_sigterm_handler);

	pqsignal(SIGQUIT, quickdie);
	InitializeTimeouts();		/* establishes SIGALRM handler */

	pqsignal(SIGPIPE, SIG_IGN);
	pqsignal(SIGUSR1, procsignal_sigusr1_handler);
	pqsignal(SIGUSR2, avl_sigusr2_handler);
	pqsignal(SIGFPE, FloatExceptionHandler);
	pqsignal(SIGCHLD, SIG_DFL);

	/* Early initialization */
	BaseInit();

	/*
	 * Create a per-backend PGPROC struct in shared memory, except in the
	 * EXEC_BACKEND case where this was done in SubPostmasterMain. We must do
	 * this before we can use LWLocks (and in the EXEC_BACKEND case we already
	 * had to do some stuff with LWLocks).
	 */
#ifndef EXEC_BACKEND
	InitProcess();
#endif

	InitPostgres(NULL, InvalidOid, NULL, NULL);

	SetProcessingMode(NormalProcessing);

	/*
	 * Create a memory context that we will do all our work in.  We do this so
	 * that we can reset the context during error recovery and thereby avoid
	 * possible memory leaks.
	 */
	AutovacMemCxt = AllocSetContextCreate(TopMemoryContext,
										  "Autovacuum Launcher",
										  ALLOCSET_DEFAULT_MINSIZE,
										  ALLOCSET_DEFAULT_INITSIZE,
										  ALLOCSET_DEFAULT_MAXSIZE);
	MemoryContextSwitchTo(AutovacMemCxt);

	/*
	 * If an exception is encountered, processing resumes here.
	 *
	 * This code is a stripped down version of PostgresMain error recovery.
	 */
	if (sigsetjmp(local_sigjmp_buf, 1) != 0)
	{
		/* since not using PG_TRY, must reset error stack by hand */
		error_context_stack = NULL;

		/* Prevents interrupts while cleaning up */
		HOLD_INTERRUPTS();

		/* Forget any pending QueryCancel or timeout request */
		QueryCancelPending = false;
		disable_all_timeouts(false);
		QueryCancelPending = false;		/* again in case timeout occurred */

		/* Report the error to the server log */
		EmitErrorReport();

		/* Abort the current transaction in order to recover */
		AbortCurrentTransaction();

		/*
		 * Now return to normal top-level context and clear ErrorContext for
		 * next time.
		 */
		MemoryContextSwitchTo(AutovacMemCxt);
		FlushErrorState();

		/* Flush any leaked data in the top-level context */
		MemoryContextResetAndDeleteChildren(AutovacMemCxt);

		/* don't leave dangling pointers to freed memory */
		DatabaseListCxt = NULL;
		dlist_init(&DatabaseList);

		/*
		 * Make sure pgstat also considers our stat data as gone.  Note: we
		 * mustn't use autovac_refresh_stats here.
		 */
		pgstat_clear_snapshot();

		/* Now we can allow interrupts again */
		RESUME_INTERRUPTS();

		/*
		 * Sleep at least 1 second after any error.  We don't want to be
		 * filling the error logs as fast as we can.
		 */
		pg_usleep(1000000L);
	}

	/* We can now handle ereport(ERROR) */
	PG_exception_stack = &local_sigjmp_buf;

	/* must unblock signals before calling rebuild_database_list */
	PG_SETMASK(&UnBlockSig);

	/*
	 * Force zero_damaged_pages OFF in the autovac process, even if it is set
	 * in postgresql.conf.	We don't really want such a dangerous option being
	 * applied non-interactively.
	 */
	SetConfigOption("zero_damaged_pages", "false", PGC_SUSET, PGC_S_OVERRIDE);

	/*
	 * Force statement_timeout and lock_timeout to zero to avoid letting these
	 * settings prevent regular maintenance from being executed.
	 */
	SetConfigOption("statement_timeout", "0", PGC_SUSET, PGC_S_OVERRIDE);
	SetConfigOption("lock_timeout", "0", PGC_SUSET, PGC_S_OVERRIDE);

	/*
	 * Force default_transaction_isolation to READ COMMITTED.  We don't want
	 * to pay the overhead of serializable mode, nor add any risk of causing
	 * deadlocks or delaying other transactions.
	 */
	SetConfigOption("default_transaction_isolation", "read committed",
					PGC_SUSET, PGC_S_OVERRIDE);


/*
 * In GPDB, we only want an autovacuum worker to start once we know
 * there is a database to vacuum. Therefore, we never want emergency mode
 * to start a worker immediately.
 *
 * Note: when the emergency mode is running it is possible to continuously
 * start an autovacuum worker. Within the worker, the PMSIGNAL_START_AUTOVAC_LAUNCHER
 * signal is sent when a database is found that is old enough to be vacuumed. If
 * the database chosen is connectable, the launcher will never select it and the
 * worker will continue to signal for a new launcher.
 */
#if 0
	/* in emergency mode, just start a worker and go away */
	if (!AutoVacuumingActive())
	{
		do_start_worker();
		proc_exit(0);			/* done */
	}
#endif

	AutoVacuumShmem->av_launcherpid = MyProcPid;

	/*
	 * Create the initial database list.  The invariant we want this list to
	 * keep is that it's ordered by decreasing next_time.  As soon as an entry
	 * is updated to a higher time, it will be moved to the front (which is
	 * correct because the only operation is to add autovacuum_naptime to the
	 * entry, and time always increases).
	 */
	rebuild_database_list(InvalidOid);

	for (;;)
	{
		struct timeval nap;
		TimestampTz current_time = 0;
		bool		can_launch;
		int			rc;

		/*
		 * This loop is a bit different from the normal use of WaitLatch,
		 * because we'd like to sleep before the first launch of a child
		 * process.  So it's WaitLatch, then ResetLatch, then check for
		 * wakening conditions.
		 */

		launcher_determine_sleep(!dlist_is_empty(&AutoVacuumShmem->av_freeWorkers),
								 false, &nap);

		/* Allow sinval catchup interrupts while sleeping */
		EnableCatchupInterrupt();

		/*
		 * Wait until naptime expires or we get some type of signal (all the
		 * signal handlers will wake us by calling SetLatch).
		 */
		rc = WaitLatch(&MyProc->procLatch,
					   WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
					   (nap.tv_sec * 1000L) + (nap.tv_usec / 1000L));

		ResetLatch(&MyProc->procLatch);

		DisableCatchupInterrupt();

		/*
		 * Emergency bailout if postmaster has died.  This is to avoid the
		 * necessity for manual cleanup of all postmaster children.
		 */
		if (rc & WL_POSTMASTER_DEATH)
			proc_exit(1);

		/* the normal shutdown case */
		if (got_SIGTERM)
			break;

		if (got_SIGHUP)
		{
			got_SIGHUP = false;
			ProcessConfigFile(PGC_SIGHUP);

			/* shutdown requested in config file? */
			if (!AutoVacuumingActive())
				break;

			/* rebalance in case the default cost parameters changed */
			LWLockAcquire(AutovacuumLock, LW_EXCLUSIVE);
			autovac_balance_cost();
			LWLockRelease(AutovacuumLock);

			/* rebuild the list in case the naptime changed */
			rebuild_database_list(InvalidOid);
		}

		/*
		 * a worker finished, or postmaster signalled failure to start a
		 * worker
		 */
		if (got_SIGUSR2)
		{
			got_SIGUSR2 = false;

			/* rebalance cost limits, if needed */
			if (AutoVacuumShmem->av_signal[AutoVacRebalance])
			{
				LWLockAcquire(AutovacuumLock, LW_EXCLUSIVE);
				AutoVacuumShmem->av_signal[AutoVacRebalance] = false;
				autovac_balance_cost();
				LWLockRelease(AutovacuumLock);
			}

			if (AutoVacuumShmem->av_signal[AutoVacForkFailed])
			{
				/*
				 * If the postmaster failed to start a new worker, we sleep
				 * for a little while and resend the signal.  The new worker's
				 * state is still in memory, so this is sufficient.  After
				 * that, we restart the main loop.
				 *
				 * XXX should we put a limit to the number of times we retry?
				 * I don't think it makes much sense, because a future start
				 * of a worker will continue to fail in the same way.
				 */
				AutoVacuumShmem->av_signal[AutoVacForkFailed] = false;
				pg_usleep(1000000L);	/* 1s */
				SendPostmasterSignal(PMSIGNAL_START_AUTOVAC_WORKER);
				continue;
			}
		}

		/*
		 * There are some conditions that we need to check before trying to
		 * start a launcher.  First, we need to make sure that there is a
		 * launcher slot available.  Second, we need to make sure that no
		 * other worker failed while starting up.
		 */

		current_time = GetCurrentTimestamp();
		LWLockAcquire(AutovacuumLock, LW_SHARED);

		can_launch = !dlist_is_empty(&AutoVacuumShmem->av_freeWorkers);

		if (AutoVacuumShmem->av_startingWorker != NULL)
		{
			int			waittime;
			WorkerInfo	worker = AutoVacuumShmem->av_startingWorker;

			/*
			 * We can't launch another worker when another one is still
			 * starting up (or failed while doing so), so just sleep for a bit
			 * more; that worker will wake us up again as soon as it's ready.
			 * We will only wait autovacuum_naptime seconds (up to a maximum
			 * of 60 seconds) for this to happen however.  Note that failure
			 * to connect to a particular database is not a problem here,
			 * because the worker removes itself from the startingWorker
			 * pointer before trying to connect.  Problems detected by the
			 * postmaster (like fork() failure) are also reported and handled
			 * differently.  The only problems that may cause this code to
			 * fire are errors in the earlier sections of AutoVacWorkerMain,
			 * before the worker removes the WorkerInfo from the
			 * startingWorker pointer.
			 */
			waittime = Min(autovacuum_naptime, 60) * 1000;
			if (TimestampDifferenceExceeds(worker->wi_launchtime, current_time,
										   waittime))
			{
				LWLockRelease(AutovacuumLock);
				LWLockAcquire(AutovacuumLock, LW_EXCLUSIVE);

				/*
				 * No other process can put a worker in starting mode, so if
				 * startingWorker is still INVALID after exchanging our lock,
				 * we assume it's the same one we saw above (so we don't
				 * recheck the launch time).
				 */
				if (AutoVacuumShmem->av_startingWorker != NULL)
				{
					worker = AutoVacuumShmem->av_startingWorker;
					worker->wi_dboid = InvalidOid;
					worker->wi_tableoid = InvalidOid;
					worker->wi_proc = NULL;
					worker->wi_launchtime = 0;
					dlist_push_head(&AutoVacuumShmem->av_freeWorkers,
									&worker->wi_links);
					AutoVacuumShmem->av_startingWorker = NULL;
					elog(WARNING, "worker took too long to start; canceled");
				}
			}
			else
				can_launch = false;
		}
		LWLockRelease(AutovacuumLock);	/* either shared or exclusive */

		/* if we can't do anything, just go back to sleep */
		if (!can_launch)
			continue;

		/* We're OK to start a new worker */

		if (dlist_is_empty(&DatabaseList))
		{
			/*
			 * Special case when the list is empty: start a worker right away.
			 * This covers the initial case, when no database is in pgstats
			 * (thus the list is empty).  Note that the constraints in
			 * launcher_determine_sleep keep us from starting workers too
			 * quickly (at most once every autovacuum_naptime when the list is
			 * empty).
			 */
			launch_worker(current_time);
		}
		else
		{
			/*
			 * because rebuild_database_list constructs a list with most
			 * distant adl_next_worker first, we obtain our database from the
			 * tail of the list.
			 */
			avl_dbase  *avdb;

			avdb = dlist_tail_element(avl_dbase, adl_node, &DatabaseList);

			/*
			 * launch a worker if next_worker is right now or it is in the
			 * past
			 */
			if (TimestampDifferenceExceeds(avdb->adl_next_worker,
										   current_time, 0))
				launch_worker(current_time);
		}
	}

	/* Normal exit from the autovac launcher is here */
	ereport(LOG,
			(errmsg("autovacuum launcher shutting down")));
	AutoVacuumShmem->av_launcherpid = 0;

	proc_exit(0);				/* done */
}

/*
 * Determine the time to sleep, based on the database list.
 *
 * The "canlaunch" parameter indicates whether we can start a worker right now,
 * for example due to the workers being all busy.  If this is false, we will
 * cause a long sleep, which will be interrupted when a worker exits.
 */
static void
launcher_determine_sleep(bool canlaunch, bool recursing, struct timeval * nap)
{
	/*
	 * We sleep until the next scheduled vacuum.  We trust that when the
	 * database list was built, care was taken so that no entries have times
	 * in the past; if the first entry has too close a next_worker value, or a
	 * time in the past, we will sleep a small nominal time.
	 */
	if (!canlaunch)
	{
		nap->tv_sec = autovacuum_naptime;
		nap->tv_usec = 0;
	}
	else if (!dlist_is_empty(&DatabaseList))
	{
		TimestampTz current_time = GetCurrentTimestamp();
		TimestampTz next_wakeup;
		avl_dbase  *avdb;
		long		secs;
		int			usecs;

		avdb = dlist_tail_element(avl_dbase, adl_node, &DatabaseList);

		next_wakeup = avdb->adl_next_worker;
		TimestampDifference(current_time, next_wakeup, &secs, &usecs);

		nap->tv_sec = secs;
		nap->tv_usec = usecs;
	}
	else
	{
		/* list is empty, sleep for whole autovacuum_naptime seconds  */
		nap->tv_sec = autovacuum_naptime;
		nap->tv_usec = 0;
	}

	/*
	 * If the result is exactly zero, it means a database had an entry with
	 * time in the past.  Rebuild the list so that the databases are evenly
	 * distributed again, and recalculate the time to sleep.  This can happen
	 * if there are more tables needing vacuum than workers, and they all take
	 * longer to vacuum than autovacuum_naptime.
	 *
	 * We only recurse once.  rebuild_database_list should always return times
	 * in the future, but it seems best not to trust too much on that.
	 */
	if (nap->tv_sec == 0 && nap->tv_usec == 0 && !recursing)
	{
		rebuild_database_list(InvalidOid);
		launcher_determine_sleep(canlaunch, true, nap);
		return;
	}

	/* The smallest time we'll allow the launcher to sleep. */
	if (nap->tv_sec <= 0 && nap->tv_usec <= MIN_AUTOVAC_SLEEPTIME * 1000)
	{
		nap->tv_sec = 0;
		nap->tv_usec = MIN_AUTOVAC_SLEEPTIME * 1000;
	}
}

/*
 * Build an updated DatabaseList.  It must only contain databases that appear
 * in pgstats, and must be sorted by next_worker from highest to lowest,
 * distributed regularly across the next autovacuum_naptime interval.
 *
 * Receives the Oid of the database that made this list be generated (we call
 * this the "new" database, because when the database was already present on
 * the list, we expect that this function is not called at all).  The
 * preexisting list, if any, will be used to preserve the order of the
 * databases in the autovacuum_naptime period.	The new database is put at the
 * end of the interval.  The actual values are not saved, which should not be
 * much of a problem.
 */
static void
rebuild_database_list(Oid newdb)
{
	List	   *dblist;
	ListCell   *cell;
	MemoryContext newcxt;
	MemoryContext oldcxt;
	MemoryContext tmpcxt;
	HASHCTL		hctl;
	int			score;
	int			nelems;
	HTAB	   *dbhash;
	dlist_iter	iter;

	/* use fresh stats */
	autovac_refresh_stats();

	newcxt = AllocSetContextCreate(AutovacMemCxt,
								   "AV dblist",
								   ALLOCSET_DEFAULT_MINSIZE,
								   ALLOCSET_DEFAULT_INITSIZE,
								   ALLOCSET_DEFAULT_MAXSIZE);
	tmpcxt = AllocSetContextCreate(newcxt,
								   "tmp AV dblist",
								   ALLOCSET_DEFAULT_MINSIZE,
								   ALLOCSET_DEFAULT_INITSIZE,
								   ALLOCSET_DEFAULT_MAXSIZE);
	oldcxt = MemoryContextSwitchTo(tmpcxt);

	/*
	 * Implementing this is not as simple as it sounds, because we need to put
	 * the new database at the end of the list; next the databases that were
	 * already on the list, and finally (at the tail of the list) all the
	 * other databases that are not on the existing list.
	 *
	 * To do this, we build an empty hash table of scored databases.  We will
	 * start with the lowest score (zero) for the new database, then
	 * increasing scores for the databases in the existing list, in order, and
	 * lastly increasing scores for all databases gotten via
	 * get_database_list() that are not already on the hash.
	 *
	 * Then we will put all the hash elements into an array, sort the array by
	 * score, and finally put the array elements into the new doubly linked
	 * list.
	 */
	hctl.keysize = sizeof(Oid);
	hctl.entrysize = sizeof(avl_dbase);
	hctl.hash = oid_hash;
	hctl.hcxt = tmpcxt;
	dbhash = hash_create("db hash", 20, &hctl,	/* magic number here FIXME */
						 HASH_ELEM | HASH_FUNCTION | HASH_CONTEXT);

	/* start by inserting the new database */
	score = 0;
	if (OidIsValid(newdb))
	{
		avl_dbase  *db;
		PgStat_StatDBEntry *entry;

		/* only consider this database if it has a pgstat entry */
		entry = pgstat_fetch_stat_dbentry(newdb);
		if (entry != NULL)
		{
			/* we assume it isn't found because the hash was just created */
			db = hash_search(dbhash, &newdb, HASH_ENTER, NULL);

			/* hash_search already filled in the key */
			db->adl_score = score++;
			/* next_worker is filled in later */
		}
	}

	/* Now insert the databases from the existing list */
	dlist_foreach(iter, &DatabaseList)
	{
		avl_dbase  *avdb = dlist_container(avl_dbase, adl_node, iter.cur);
		avl_dbase  *db;
		bool		found;
		PgStat_StatDBEntry *entry;

		/*
		 * skip databases with no stat entries -- in particular, this gets rid
		 * of dropped databases
		 */
		entry = pgstat_fetch_stat_dbentry(avdb->adl_datid);
		if (entry == NULL)
			continue;

		db = hash_search(dbhash, &(avdb->adl_datid), HASH_ENTER, &found);

		if (!found)
		{
			/* hash_search already filled in the key */
			db->adl_score = score++;
			/* next_worker is filled in later */
		}
	}

	/* finally, insert all qualifying databases not previously inserted */
	dblist = get_database_list();
	foreach(cell, dblist)
	{
		avw_dbase  *avdb = lfirst(cell);
		avl_dbase  *db;
		bool		found;
		PgStat_StatDBEntry *entry;

		/* only consider databases with a pgstat entry */
		entry = pgstat_fetch_stat_dbentry(avdb->adw_datid);
		if (entry == NULL)
			continue;

		db = hash_search(dbhash, &(avdb->adw_datid), HASH_ENTER, &found);
		/* only update the score if the database was not already on the hash */
		if (!found)
		{
			/* hash_search already filled in the key */
			db->adl_score = score++;
			/* next_worker is filled in later */
		}
	}
	nelems = score;

	/* from here on, the allocated memory belongs to the new list */
	MemoryContextSwitchTo(newcxt);
	dlist_init(&DatabaseList);

	if (nelems > 0)
	{
		TimestampTz current_time;
		int			millis_increment;
		avl_dbase  *dbary;
		avl_dbase  *db;
		HASH_SEQ_STATUS seq;
		int			i;

		/* put all the hash elements into an array */
		dbary = palloc(nelems * sizeof(avl_dbase));

		i = 0;
		hash_seq_init(&seq, dbhash);
		while ((db = hash_seq_search(&seq)) != NULL)
			memcpy(&(dbary[i++]), db, sizeof(avl_dbase));

		/* sort the array */
		qsort(dbary, nelems, sizeof(avl_dbase), db_comparator);

		/*
		 * Determine the time interval between databases in the schedule. If
		 * we see that the configured naptime would take us to sleep times
		 * lower than our min sleep time (which launcher_determine_sleep is
		 * coded not to allow), silently use a larger naptime (but don't touch
		 * the GUC variable).
		 */
		millis_increment = 1000.0 * autovacuum_naptime / nelems;
		if (millis_increment <= MIN_AUTOVAC_SLEEPTIME)
			millis_increment = MIN_AUTOVAC_SLEEPTIME * 1.1;

		current_time = GetCurrentTimestamp();

		/*
		 * move the elements from the array into the dllist, setting the
		 * next_worker while walking the array
		 */
		for (i = 0; i < nelems; i++)
		{
			avl_dbase  *db = &(dbary[i]);

			current_time = TimestampTzPlusMilliseconds(current_time,
													   millis_increment);
			db->adl_next_worker = current_time;

			/* later elements should go closer to the head of the list */
			dlist_push_head(&DatabaseList, &db->adl_node);
		}
	}

	/* all done, clean up memory */
	if (DatabaseListCxt != NULL)
		MemoryContextDelete(DatabaseListCxt);
	MemoryContextDelete(tmpcxt);
	DatabaseListCxt = newcxt;
	MemoryContextSwitchTo(oldcxt);
}

/* qsort comparator for avl_dbase, using adl_score */
static int
db_comparator(const void *a, const void *b)
{
	if (((const avl_dbase *) a)->adl_score == ((const avl_dbase *) b)->adl_score)
		return 0;
	else
		return (((const avl_dbase *) a)->adl_score < ((const avl_dbase *) b)->adl_score) ? 1 : -1;
}

/*
 * do_start_worker
 *
 * Bare-bones procedure for starting an autovacuum worker from the launcher.
 * It determines what database to work on, sets up shared memory stuff and
 * signals postmaster to start the worker.  It fails gracefully if invoked when
 * autovacuum_workers are already active.
 *
 * Return value is the OID of the database that the worker is going to process,
 * or InvalidOid if no worker was actually started.
 */
static Oid
do_start_worker(void)
{
	List	   *dblist;
	ListCell   *cell;
	TransactionId xidForceLimit;
	MultiXactId multiForceLimit;
	bool		for_xid_wrap;
	bool		for_multi_wrap;
	avw_dbase  *avdb;
	TimestampTz	current_time;
	bool		skipit = false;
	Oid			retval = InvalidOid;
	MemoryContext tmpcxt,
				oldcxt;

	/* return quickly when there are no free workers */
	LWLockAcquire(AutovacuumLock, LW_SHARED);
	if (dlist_is_empty(&AutoVacuumShmem->av_freeWorkers))
	{
		LWLockRelease(AutovacuumLock);
		return InvalidOid;
	}
	LWLockRelease(AutovacuumLock);

	/*
	 * Create and switch to a temporary context to avoid leaking the memory
	 * allocated for the database list.
	 */
	tmpcxt = AllocSetContextCreate(CurrentMemoryContext,
								   "Start worker tmp cxt",
								   ALLOCSET_DEFAULT_MINSIZE,
								   ALLOCSET_DEFAULT_INITSIZE,
								   ALLOCSET_DEFAULT_MAXSIZE);
	oldcxt = MemoryContextSwitchTo(tmpcxt);

	/* use fresh stats */
	autovac_refresh_stats();

	/* Get a list of databases */
	dblist = get_database_list();

	/*
	 * Determine the oldest datfrozenxid/relfrozenxid that we will allow to
	 * pass without forcing a vacuum.  (This limit can be tightened for
	 * particular tables, but not loosened.)
	 */
	recentXid = ReadNewTransactionId();
	xidForceLimit = recentXid - autovacuum_freeze_max_age;
	/* ensure it's a "normal" XID, else TransactionIdPrecedes misbehaves */
	/* this can cause the limit to go backwards by 3, but that's OK */
	if (xidForceLimit < FirstNormalTransactionId)
		xidForceLimit -= FirstNormalTransactionId;

	/* Also determine the oldest datminmxid we will consider. */
	recentMulti = ReadNextMultiXactId();
	multiForceLimit = recentMulti - autovacuum_freeze_max_age;
	if (multiForceLimit < FirstMultiXactId)
		multiForceLimit -= FirstMultiXactId;

	/*
	 * Choose a database to connect to.  We pick the database that was least
	 * recently auto-vacuumed, or one that needs vacuuming to prevent Xid
	 * wraparound-related data loss.  If any db at risk of Xid wraparound is
	 * found, we pick the one with oldest datfrozenxid, independently of
	 * autovacuum times; similarly we pick the one with the oldest datminmxid
	 * if any is in MultiXactId wraparound.  Note that those in Xid wraparound
	 * danger are given more priority than those in multi wraparound danger.
	 *
	 * Note that a database with no stats entry is not considered, except for
	 * Xid wraparound purposes.  The theory is that if no one has ever
	 * connected to it since the stats were last initialized, it doesn't need
	 * vacuuming.
	 *
	 * XXX This could be improved if we had more info about whether it needs
	 * vacuuming before connecting to it.  Perhaps look through the pgstats
	 * data for the database's tables?  One idea is to keep track of the
	 * number of new and dead tuples per database in pgstats.  However it
	 * isn't clear how to construct a metric that measures that and not cause
	 * starvation for less busy databases.
	 */
	avdb = NULL;
	for_xid_wrap = false;
	for_multi_wrap = false;
	current_time = GetCurrentTimestamp();
	foreach(cell, dblist)
	{
		avw_dbase  *tmp = lfirst(cell);
		dlist_iter	iter;

		/* Check to see if this one is at risk of wraparound */
		if (TransactionIdPrecedes(tmp->adw_frozenxid, xidForceLimit))
		{
			if (avdb == NULL ||
				TransactionIdPrecedes(tmp->adw_frozenxid,
									  avdb->adw_frozenxid))
				avdb = tmp;
			for_xid_wrap = true;
			continue;
		}
		else if (for_xid_wrap)
			continue;			/* ignore not-at-risk DBs */
		else if (MultiXactIdPrecedes(tmp->adw_frozenmulti, multiForceLimit))
		{
			if (avdb == NULL ||
				MultiXactIdPrecedes(tmp->adw_frozenmulti,
									avdb->adw_frozenmulti))
				avdb = tmp;
			for_multi_wrap = true;
			continue;
		}
		else if (for_multi_wrap)
			continue;			/* ignore not-at-risk DBs */

		/* Find pgstat entry if any */
		tmp->adw_entry = pgstat_fetch_stat_dbentry(tmp->adw_datid);

		/*
		 * Skip a database with no pgstat entry; it means it hasn't seen any
		 * activity.
		 */
		if (!tmp->adw_entry)
			continue;

		/*
		 * Also, skip a database that appears on the database list as having
		 * been processed recently (less than autovacuum_naptime seconds ago).
		 * We do this so that we don't select a database which we just
		 * selected, but that pgstat hasn't gotten around to updating the last
		 * autovacuum time yet.
		 */
		skipit = false;

		dlist_reverse_foreach(iter, &DatabaseList)
		{
			avl_dbase  *dbp = dlist_container(avl_dbase, adl_node, iter.cur);

			if (dbp->adl_datid == tmp->adw_datid)
			{
				/*
				 * Skip this database if its next_worker value falls between
				 * the current time and the current time plus naptime.
				 */
				if (!TimestampDifferenceExceeds(dbp->adl_next_worker,
												current_time, 0) &&
					!TimestampDifferenceExceeds(current_time,
												dbp->adl_next_worker,
												autovacuum_naptime * 1000))
					skipit = true;

				break;
			}
		}
		if (skipit)
			continue;

		/*
		 * Remember the db with oldest autovac time.  (If we are here, both
		 * tmp->entry and db->entry must be non-null.)
		 */
		if (avdb == NULL ||
			tmp->adw_entry->last_autovac_time < avdb->adw_entry->last_autovac_time)
			avdb = tmp;
	}

	/* Found a database -- process it */
	if (avdb != NULL)
	{
		WorkerInfo	worker;
		dlist_node *wptr;

		LWLockAcquire(AutovacuumLock, LW_EXCLUSIVE);

		/*
		 * Get a worker entry from the freelist.  We checked above, so there
		 * really should be a free slot.
		 */
		wptr = dlist_pop_head_node(&AutoVacuumShmem->av_freeWorkers);

		worker = dlist_container(WorkerInfoData, wi_links, wptr);
		worker->wi_dboid = avdb->adw_datid;
		worker->wi_proc = NULL;
		worker->wi_launchtime = GetCurrentTimestamp();

		AutoVacuumShmem->av_startingWorker = worker;

		LWLockRelease(AutovacuumLock);

		SendPostmasterSignal(PMSIGNAL_START_AUTOVAC_WORKER);

		retval = avdb->adw_datid;
	}
	else if (skipit)
	{
		/*
		 * If we skipped all databases on the list, rebuild it, because it
		 * probably contains a dropped database.
		 */
		rebuild_database_list(InvalidOid);
	}

	MemoryContextSwitchTo(oldcxt);
	MemoryContextDelete(tmpcxt);

	return retval;
}

/*
 * launch_worker
 *
 * Wrapper for starting a worker from the launcher.  Besides actually starting
 * it, update the database list to reflect the next time that another one will
 * need to be started on the selected database.  The actual database choice is
 * left to do_start_worker.
 *
 * This routine is also expected to insert an entry into the database list if
 * the selected database was previously absent from the list.
 */
static void
launch_worker(TimestampTz now)
{
	Oid			dbid;
	dlist_iter	iter;

	dbid = do_start_worker();
	if (OidIsValid(dbid))
	{
		bool		found = false;

		/*
		 * Walk the database list and update the corresponding entry.  If the
		 * database is not on the list, we'll recreate the list.
		 */
		dlist_foreach(iter, &DatabaseList)
		{
			avl_dbase  *avdb = dlist_container(avl_dbase, adl_node, iter.cur);

			if (avdb->adl_datid == dbid)
			{
				found = true;

				/*
				 * add autovacuum_naptime seconds to the current time, and use
				 * that as the new "next_worker" field for this database.
				 */
				avdb->adl_next_worker =
					TimestampTzPlusMilliseconds(now, autovacuum_naptime * 1000);

				dlist_move_head(&DatabaseList, iter.cur);
				break;
			}
		}

		/*
		 * If the database was not present in the database list, we rebuild
		 * the list.  It's possible that the database does not get into the
		 * list anyway, for example if it's a database that doesn't have a
		 * pgstat entry, but this is not a problem because we don't want to
		 * schedule workers regularly into those in any case.
		 */
		if (!found)
			rebuild_database_list(dbid);
	}
}

/*
 * Called from postmaster to signal a failure to fork a process to become
 * worker.	The postmaster should kill(SIGUSR2) the launcher shortly
 * after calling this function.
 */
void
AutoVacWorkerFailed(void)
{
	AutoVacuumShmem->av_signal[AutoVacForkFailed] = true;
}

/* SIGHUP: set flag to re-read config file at next convenient time */
static void
avl_sighup_handler(SIGNAL_ARGS)
{
	int			save_errno = errno;

	got_SIGHUP = true;
	if (MyProc)
		SetLatch(&MyProc->procLatch);

	errno = save_errno;
}

/* SIGUSR2: a worker is up and running, or just finished, or failed to fork */
static void
avl_sigusr2_handler(SIGNAL_ARGS)
{
	int			save_errno = errno;

	got_SIGUSR2 = true;
	if (MyProc)
		SetLatch(&MyProc->procLatch);

	errno = save_errno;
}

/* SIGTERM: time to die */
static void
avl_sigterm_handler(SIGNAL_ARGS)
{
	int			save_errno = errno;

	got_SIGTERM = true;
	if (MyProc)
		SetLatch(&MyProc->procLatch);

	errno = save_errno;
}


/********************************************************************
 *					  AUTOVACUUM WORKER CODE
 ********************************************************************/

#ifdef EXEC_BACKEND
/*
 * forkexec routines for the autovacuum worker.
 *
 * Format up the arglist, then fork and exec.
 */
static pid_t
avworker_forkexec(void)
{
	char	   *av[10];
	int			ac = 0;

	av[ac++] = "postgres";
	av[ac++] = "--forkavworker";
	av[ac++] = NULL;			/* filled in by postmaster_forkexec */
	av[ac] = NULL;

	Assert(ac < lengthof(av));

	return postmaster_forkexec(ac, av);
}

/*
 * We need this set from the outside, before InitProcess is called
 */
void
AutovacuumWorkerIAm(void)
{
	am_autovacuum_worker = true;
}
#endif

/*
 * Main entry point for autovacuum worker process.
 *
 * This code is heavily based on pgarch.c, q.v.
 */
int
StartAutoVacWorker(void)
{
	pid_t		worker_pid;

#ifdef EXEC_BACKEND
	switch ((worker_pid = avworker_forkexec()))
#else
	switch ((worker_pid = fork_process()))
#endif
	{
		case -1:
			ereport(LOG,
					(errmsg("could not fork autovacuum worker process: %m")));
			return 0;

#ifndef EXEC_BACKEND
		case 0:
			/* in postmaster child ... */
			/* Close the postmaster's sockets */
			ClosePostmasterPorts(false);

			/* Lose the postmaster's on-exit routines */
			on_exit_reset();

			AutoVacWorkerMain(0, NULL);
			break;
#endif
		default:
			return (int) worker_pid;
	}

	/* shouldn't get here */
	return 0;
}

/*
 * AutoVacWorkerMain
 */
NON_EXEC_STATIC void
AutoVacWorkerMain(int argc, char *argv[])
{
	sigjmp_buf	local_sigjmp_buf;
	Oid			dbid;

	/* we are a postmaster subprocess now */
	IsUnderPostmaster = true;
	am_autovacuum_worker = true;

	/* MPP-4990: Autovacuum always runs as utility-mode */
	Gp_role = GP_ROLE_UTILITY;

	/* reset MyProcPid */
	MyProcPid = getpid();

	/* record Start Time for logging */
	MyStartTime = time(NULL);

	/* Identify myself via ps */
	init_ps_display("autovacuum worker process", "", "", "");

	SetProcessingMode(InitProcessing);

	/*
	 * If possible, make this process a group leader, so that the postmaster
	 * can signal any child processes too.	(autovacuum probably never has any
	 * child processes, but for consistency we make all postmaster child
	 * processes do this.)
	 */
#ifdef HAVE_SETSID
	if (setsid() < 0)
		elog(FATAL, "setsid() failed: %m");
#endif

	/*
	 * Set up signal handlers.	We operate on databases much like a regular
	 * backend, so we use the same signal handling.  See equivalent code in
	 * tcop/postgres.c.
	 *
	 * Currently, we don't pay attention to postgresql.conf changes that
	 * happen during a single daemon iteration, so we can ignore SIGHUP.
	 */
	pqsignal(SIGHUP, SIG_IGN);

	/*
	 * SIGINT is used to signal canceling the current table's vacuum; SIGTERM
	 * means abort and exit cleanly, and SIGQUIT means abandon ship.
	 */
	pqsignal(SIGINT, StatementCancelHandler);
	pqsignal(SIGTERM, die);
	pqsignal(SIGQUIT, quickdie);
	InitializeTimeouts();		/* establishes SIGALRM handler */

	pqsignal(SIGPIPE, SIG_IGN);
	pqsignal(SIGUSR1, procsignal_sigusr1_handler);
	pqsignal(SIGUSR2, SIG_IGN);
	pqsignal(SIGFPE, FloatExceptionHandler);
	pqsignal(SIGCHLD, SIG_DFL);

	/* Early initialization */
	BaseInit();

	/*
	 * Create a per-backend PGPROC struct in shared memory, except in the
	 * EXEC_BACKEND case where this was done in SubPostmasterMain. We must do
	 * this before we can use LWLocks (and in the EXEC_BACKEND case we already
	 * had to do some stuff with LWLocks).
	 */
#ifndef EXEC_BACKEND
	InitProcess();
#endif

	/*
	 * If an exception is encountered, processing resumes here.
	 *
	 * See notes in postgres.c about the design of this coding.
	 */
	if (sigsetjmp(local_sigjmp_buf, 1) != 0)
	{
		/* Prevents interrupts while cleaning up */
		HOLD_INTERRUPTS();

		/* Report the error to the server log */
		EmitErrorReport();

		/*
		 * We can now go away.	Note that because we called InitProcess, a
		 * callback was registered to do ProcKill, which will clean up
		 * necessary state.
		 */
		proc_exit(0);
	}

	/* We can now handle ereport(ERROR) */
	PG_exception_stack = &local_sigjmp_buf;

	PG_SETMASK(&UnBlockSig);

	/*
	 * Force zero_damaged_pages OFF in the autovac process, even if it is set
	 * in postgresql.conf.	We don't really want such a dangerous option being
	 * applied non-interactively.
	 */
	SetConfigOption("zero_damaged_pages", "false", PGC_SUSET, PGC_S_OVERRIDE);

	/*
	 * Force statement_timeout and lock_timeout to zero to avoid letting these
	 * settings prevent regular maintenance from being executed.
	 */
	SetConfigOption("statement_timeout", "0", PGC_SUSET, PGC_S_OVERRIDE);
	SetConfigOption("lock_timeout", "0", PGC_SUSET, PGC_S_OVERRIDE);

	/*
	 * Force default_transaction_isolation to READ COMMITTED.  We don't want
	 * to pay the overhead of serializable mode, nor add any risk of causing
	 * deadlocks or delaying other transactions.
	 */
	SetConfigOption("default_transaction_isolation", "read committed",
					PGC_SUSET, PGC_S_OVERRIDE);

	/*
	 * Force synchronous replication off to allow regular maintenance even if
	 * we are waiting for standbys to connect. This is important to ensure we
	 * aren't blocked from performing anti-wraparound tasks.
	 */
	if (synchronous_commit > SYNCHRONOUS_COMMIT_LOCAL_FLUSH)
		SetConfigOption("synchronous_commit", "local",
						PGC_SUSET, PGC_S_OVERRIDE);

	/*
	 * Get the info about the database we're going to work on.
	 */
	LWLockAcquire(AutovacuumLock, LW_EXCLUSIVE);

	/*
	 * beware of startingWorker being INVALID; this should normally not
	 * happen, but if a worker fails after forking and before this, the
	 * launcher might have decided to remove it from the queue and start
	 * again.
	 */
	if (AutoVacuumShmem->av_startingWorker != NULL)
	{
		MyWorkerInfo = AutoVacuumShmem->av_startingWorker;
		dbid = MyWorkerInfo->wi_dboid;
		MyWorkerInfo->wi_proc = MyProc;

		/* insert into the running list */
		dlist_push_head(&AutoVacuumShmem->av_runningWorkers,
						&MyWorkerInfo->wi_links);

		/*
		 * remove from the "starting" pointer, so that the launcher can start
		 * a new worker if required
		 */
		AutoVacuumShmem->av_startingWorker = NULL;
		LWLockRelease(AutovacuumLock);

		on_shmem_exit(FreeWorkerInfo, 0);

		/* wake up the launcher */
		if (AutoVacuumShmem->av_launcherpid != 0)
			kill(AutoVacuumShmem->av_launcherpid, SIGUSR2);
	}
	else
	{
		/* no worker entry for me, go away */
		elog(WARNING, "autovacuum worker started without a worker entry");
		dbid = InvalidOid;
		LWLockRelease(AutovacuumLock);
	}

	if (OidIsValid(dbid))
	{
		char		dbname[NAMEDATALEN];

		/*
		 * Report autovac startup to the stats collector.  We deliberately do
		 * this before InitPostgres, so that the last_autovac_time will get
		 * updated even if the connection attempt fails.  This is to prevent
		 * autovac from getting "stuck" repeatedly selecting an unopenable
		 * database, rather than making any progress on stuff it can connect
		 * to.
		 */
		pgstat_report_autovac(dbid);

		/*
		 * Connect to the selected database
		 *
		 * Note: if we have selected a just-deleted database (due to using
		 * stale stats info), we'll fail and exit here.
		 */
		InitPostgres(NULL, dbid, NULL, dbname);
		SetProcessingMode(NormalProcessing);
		set_ps_display(dbname, false);
		ereport(LOG,
				(errmsg("autovacuum: processing database \"%s\"", dbname)));

		SIMPLE_FAULT_INJECTOR(AutoVacWorkerBeforeDoAutovacuum);

		if (PostAuthDelay)
			pg_usleep(PostAuthDelay * 1000000L);

		/* And do an appropriate amount of work */
		recentXid = ReadNewTransactionId();
		recentMulti = ReadNextMultiXactId();
		do_autovacuum();
	}

	/*
	 * The launcher will be notified of my death in ProcKill, *if* we managed
	 * to get a worker slot at all
	 */

	/* All done, go away */
	proc_exit(0);
}

/*
 * Return a WorkerInfo to the free list
 */
static void
FreeWorkerInfo(int code, Datum arg)
{
	if (MyWorkerInfo != NULL)
	{
		LWLockAcquire(AutovacuumLock, LW_EXCLUSIVE);

		/*
		 * Wake the launcher up so that he can launch a new worker immediately
		 * if required.  We only save the launcher's PID in local memory here;
		 * the actual signal will be sent when the PGPROC is recycled.	Note
		 * that we always do this, so that the launcher can rebalance the cost
		 * limit setting of the remaining workers.
		 *
		 * We somewhat ignore the risk that the launcher changes its PID
		 * between us reading it and the actual kill; we expect ProcKill to be
		 * called shortly after us, and we assume that PIDs are not reused too
		 * quickly after a process exits.
		 */
		AutovacuumLauncherPid = AutoVacuumShmem->av_launcherpid;

		dlist_delete(&MyWorkerInfo->wi_links);
		MyWorkerInfo->wi_dboid = InvalidOid;
		MyWorkerInfo->wi_tableoid = InvalidOid;
		MyWorkerInfo->wi_proc = NULL;
		MyWorkerInfo->wi_launchtime = 0;
		MyWorkerInfo->wi_cost_delay = 0;
		MyWorkerInfo->wi_cost_limit = 0;
		MyWorkerInfo->wi_cost_limit_base = 0;
		dlist_push_head(&AutoVacuumShmem->av_freeWorkers,
						&MyWorkerInfo->wi_links);
		/* not mine anymore */
		MyWorkerInfo = NULL;

		/*
		 * now that we're inactive, cause a rebalancing of the surviving
		 * workers
		 */
		AutoVacuumShmem->av_signal[AutoVacRebalance] = true;
		LWLockRelease(AutovacuumLock);
	}
}

/*
 * Update the cost-based delay parameters, so that multiple workers consume
 * each a fraction of the total available I/O.
 */
void
AutoVacuumUpdateDelay(void)
{
	if (MyWorkerInfo)
	{
		VacuumCostDelay = MyWorkerInfo->wi_cost_delay;
		VacuumCostLimit = MyWorkerInfo->wi_cost_limit;
	}
}

/*
 * autovac_balance_cost
 *		Recalculate the cost limit setting for each active worker.
 *
 * Caller must hold the AutovacuumLock in exclusive mode.
 */
static void
autovac_balance_cost(void)
{
	/*
	 * The idea here is that we ration out I/O equally.  The amount of I/O
	 * that a worker can consume is determined by cost_limit/cost_delay, so we
	 * try to equalize those ratios rather than the raw limit settings.
	 *
	 * note: in cost_limit, zero also means use value from elsewhere, because
	 * zero is not a valid value.
	 */
	int			vac_cost_limit = (autovacuum_vac_cost_limit > 0 ?
								autovacuum_vac_cost_limit : VacuumCostLimit);
	int			vac_cost_delay = (autovacuum_vac_cost_delay >= 0 ?
								autovacuum_vac_cost_delay : VacuumCostDelay);
	double		cost_total;
	double		cost_avail;
	dlist_iter	iter;

	/* not set? nothing to do */
	if (vac_cost_limit <= 0 || vac_cost_delay <= 0)
		return;

	/* caculate the total base cost limit of active workers */
	cost_total = 0.0;
	dlist_foreach(iter, &AutoVacuumShmem->av_runningWorkers)
	{
		WorkerInfo	worker = dlist_container(WorkerInfoData, wi_links, iter.cur);

		if (worker->wi_proc != NULL &&
			worker->wi_cost_limit_base > 0 && worker->wi_cost_delay > 0)
			cost_total +=
				(double) worker->wi_cost_limit_base / worker->wi_cost_delay;
	}
	/* there are no cost limits -- nothing to do */
	if (cost_total <= 0)
		return;

	/*
	 * Adjust cost limit of each active worker to balance the total of cost
	 * limit to autovacuum_vacuum_cost_limit.
	 */
	cost_avail = (double) vac_cost_limit / vac_cost_delay;
	dlist_foreach(iter, &AutoVacuumShmem->av_runningWorkers)
	{
		WorkerInfo	worker = dlist_container(WorkerInfoData, wi_links, iter.cur);

		if (worker->wi_proc != NULL &&
			worker->wi_cost_limit_base > 0 && worker->wi_cost_delay > 0)
		{
			int			limit = (int)
			(cost_avail * worker->wi_cost_limit_base / cost_total);

			/*
			 * We put a lower bound of 1 on the cost_limit, to avoid division-
			 * by-zero in the vacuum code.	Also, in case of roundoff trouble
			 * in these calculations, let's be sure we don't ever set
			 * cost_limit to more than the base value.
			 */
			worker->wi_cost_limit = Max(Min(limit,
											worker->wi_cost_limit_base),
										1);

			elog(DEBUG2, "autovac_balance_cost(pid=%u db=%u, rel=%u, cost_limit=%d, cost_limit_base=%d, cost_delay=%d)",
				 worker->wi_proc->pid, worker->wi_dboid, worker->wi_tableoid,
				 worker->wi_cost_limit, worker->wi_cost_limit_base,
				 worker->wi_cost_delay);
		}
	}
}

/*
 * get_database_list
 *		Return a list of all databases found in pg_database.
 *
 * The list and associated data is allocated in the caller's memory context,
 * which is in charge of ensuring that it's properly cleaned up afterwards.
 *
 * Note: this is the only function in which the autovacuum launcher uses a
 * transaction.  Although we aren't attached to any particular database and
 * therefore can't access most catalogs, we do have enough infrastructure
 * to do a seqscan on pg_database.
 */
static List *
get_database_list(void)
{
	List	   *dblist = NIL;
	Relation	rel;
	HeapScanDesc scan;
	HeapTuple	tup;
	MemoryContext resultcxt;

	/* This is the context that we will allocate our output data in */
	resultcxt = CurrentMemoryContext;

	/*
	 * Start a transaction so we can access pg_database, and get a snapshot.
	 * We don't have a use for the snapshot itself, but we're interested in
	 * the secondary effect that it sets RecentGlobalXmin.	(This is critical
	 * for anything that reads heap pages, because HOT may decide to prune
	 * them even if the process doesn't attempt to modify any tuples.)
	 */
	StartTransactionCommand();
	(void) GetTransactionSnapshot();

	rel = heap_open(DatabaseRelationId, AccessShareLock);
	scan = heap_beginscan(rel, SnapshotNow, 0, NULL);

	while (HeapTupleIsValid(tup = heap_getnext(scan, ForwardScanDirection)))
	{
		Form_pg_database pgdatabase = (Form_pg_database) GETSTRUCT(tup);
		avw_dbase  *avdb;
		MemoryContext oldcxt;

		/*
		 * Allocate our results in the caller's context, not the
		 * transaction's. We do this inside the loop, and restore the original
		 * context at the end, so that leaky things like heap_getnext() are
		 * not called in a potentially long-lived context.
		 */
		oldcxt = MemoryContextSwitchTo(resultcxt);

		if (GpAutovacuumEnabled(pgdatabase))
			continue;

		avdb = (avw_dbase *) palloc(sizeof(avw_dbase));

		avdb->adw_datid = HeapTupleGetOid(tup);
		avdb->adw_name = pstrdup(NameStr(pgdatabase->datname));
		avdb->adw_frozenxid = pgdatabase->datfrozenxid;
		avdb->adw_frozenmulti = pgdatabase->datminmxid;
		/* this gets set later: */
		avdb->adw_entry = NULL;

		dblist = lappend(dblist, avdb);
		MemoryContextSwitchTo(oldcxt);
	}

	heap_endscan(scan);
	heap_close(rel, AccessShareLock);

	CommitTransactionCommand();

	return dblist;
}

/*
 * Process a database table-by-table
 *
 * Note that CHECK_FOR_INTERRUPTS is supposed to be used in certain spots in
 * order not to ignore shutdown commands for too long.
 */
static void
do_autovacuum(void)
{
	Relation	classRel;
	HeapTuple	tuple;
	HeapScanDesc relScan;
	Form_pg_database dbForm;
	List	   *table_oids = NIL;
	HASHCTL		ctl;
	HTAB	   *table_toast_map;
	ListCell   *volatile cell;
	PgStat_StatDBEntry *shared;
	PgStat_StatDBEntry *dbentry;
	BufferAccessStrategy bstrategy;
	ScanKeyData key;
	TupleDesc	pg_class_desc;

	/*
	 * StartTransactionCommand and CommitTransactionCommand will automatically
	 * switch to other contexts.  We need this one to keep the list of
	 * relations to vacuum/analyze across transactions.
	 */
	AutovacMemCxt = AllocSetContextCreate(TopMemoryContext,
										  "AV worker",
										  ALLOCSET_DEFAULT_MINSIZE,
										  ALLOCSET_DEFAULT_INITSIZE,
										  ALLOCSET_DEFAULT_MAXSIZE);
	MemoryContextSwitchTo(AutovacMemCxt);

	/*
	 * may be NULL if we couldn't find an entry (only happens if we are
	 * forcing a vacuum for anti-wrap purposes).
	 */
	dbentry = pgstat_fetch_stat_dbentry(MyDatabaseId);

	/* Start a transaction so our commands have one to play into. */
	StartTransactionCommand();

	/*
	 * Clean up any dead statistics collector entries for this DB. We always
	 * want to do this exactly once per DB-processing cycle, even if we find
	 * nothing worth vacuuming in the database.
	 */
	pgstat_vacuum_stat();

	/*
	 * Find the pg_database entry and select the default freeze ages. We use
	 * zero in template and nonconnectable databases, else the system-wide
	 * default.
	 */
	tuple = SearchSysCache1(DATABASEOID, ObjectIdGetDatum(MyDatabaseId));
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for database %u", MyDatabaseId);
	dbForm = (Form_pg_database) GETSTRUCT(tuple);

	if (dbForm->datistemplate || !dbForm->datallowconn)
	{
		default_freeze_min_age = 0;
		default_freeze_table_age = 0;
	}
	else
	{
		default_freeze_min_age = vacuum_freeze_min_age;
		default_freeze_table_age = vacuum_freeze_table_age;
	}

	ReleaseSysCache(tuple);

	/* StartTransactionCommand changed elsewhere */
	MemoryContextSwitchTo(AutovacMemCxt);

	/* The database hash where pgstat keeps shared relations */
	shared = pgstat_fetch_stat_dbentry(InvalidOid);

	classRel = heap_open(RelationRelationId, AccessShareLock);

	/* create a copy so we can use it after closing pg_class */
	pg_class_desc = CreateTupleDescCopy(RelationGetDescr(classRel));

	/* create hash table for toast <-> main relid mapping */
	MemSet(&ctl, 0, sizeof(ctl));
	ctl.keysize = sizeof(Oid);
	ctl.entrysize = sizeof(av_relation);
	ctl.hash = oid_hash;

	table_toast_map = hash_create("TOAST to main relid map",
								  100,
								  &ctl,
								  HASH_ELEM | HASH_FUNCTION);

	/*
	 * Scan pg_class to determine which tables to vacuum.
	 *
	 * We do this in two passes: on the first one we collect the list of plain
	 * relations and materialized views, and on the second one we collect
	 * TOAST tables. The reason for doing the second pass is that during it we
	 * want to use the main relation's pg_class.reloptions entry if the TOAST
	 * table does not have any, and we cannot obtain it unless we know
	 * beforehand what's the main  table OID.
	 *
	 * We need to check TOAST tables separately because in cases with short,
	 * wide tables there might be proportionally much more activity in the
	 * TOAST table than in its parent.
	 */
	relScan = heap_beginscan(classRel, SnapshotNow, 0, NULL);

	/*
	 * On the first pass, we collect main tables to vacuum, and also the main
	 * table relid to TOAST relid mapping.
	 */
	while ((tuple = heap_getnext(relScan, ForwardScanDirection)) != NULL)
	{
		Form_pg_class classForm = (Form_pg_class) GETSTRUCT(tuple);
		PgStat_StatTabEntry *tabentry;
		AutoVacOpts *relopts;
		Oid			relid;
		bool		dovacuum;
		bool		doanalyze;
		bool		wraparound;

		if (classForm->relkind != RELKIND_RELATION &&
			classForm->relkind != RELKIND_MATVIEW)
			continue;

		relid = HeapTupleGetOid(tuple);

		/* Fetch reloptions and the pgstat entry for this table */
		relopts = extract_autovac_opts(tuple, pg_class_desc);
		tabentry = get_pgstat_tabentry_relid(relid, classForm->relisshared,
											 shared, dbentry);

		/* Check if it needs vacuum or analyze */
		relation_needs_vacanalyze(relid, relopts, classForm, tabentry,
								  &dovacuum, &doanalyze, &wraparound);

		/*
		 * Check if it is a temp table (presumably, of some other backend's).
		 * We cannot safely process other backends' temp tables.
		 */
		if (classForm->relpersistence == RELPERSISTENCE_TEMP)
		{
			int			backendID;

			/*
			 * GPDB_91_MERGE_FIXME: Autovacuum operates only on template0
			 * database in Greenplum.  We expect no temp tables in template0.
			 * Whenever we allow autovacuum to operate on user databases, we
			 * must deal with the logic to detect other backend's temp tables
			 * below.
			 */
			Assert(false);
			
			backendID = GetTempNamespaceBackendId(classForm->relnamespace);

			/* We just ignore it if the owning backend is still active */
			if (backendID == MyBackendId || BackendIdGetProc(backendID) == NULL)
			{
				/*
				 * We found an orphan temp table (which was probably left
				 * behind by a crashed backend).  If it's so old as to need
				 * vacuum for wraparound, forcibly drop it.  Otherwise just
				 * log a complaint.
				 */
				if (wraparound)
				{
					ObjectAddress object;

					ereport(LOG,
							(errmsg("autovacuum: dropping orphan temp table \"%s\".\"%s\" in database \"%s\"",
								 get_namespace_name(classForm->relnamespace),
									NameStr(classForm->relname),
									get_database_name(MyDatabaseId))));
					object.classId = RelationRelationId;
					object.objectId = relid;
					object.objectSubId = 0;
					performDeletion(&object, DROP_CASCADE, PERFORM_DELETION_INTERNAL);
				}
				else
				{
					ereport(LOG,
							(errmsg("autovacuum: found orphan temp table \"%s\".\"%s\" in database \"%s\"",
								 get_namespace_name(classForm->relnamespace),
									NameStr(classForm->relname),
									get_database_name(MyDatabaseId))));
				}
			}
		}
		else
		{
			/* relations that need work are added to table_oids */
			if (dovacuum || doanalyze)
				table_oids = lappend_oid(table_oids, relid);

			/*
			 * Remember the association for the second pass.  Note: we must do
			 * this even if the table is going to be vacuumed, because we
			 * don't automatically vacuum toast tables along the parent table.
			 */
			if (OidIsValid(classForm->reltoastrelid))
			{
				av_relation *hentry;
				bool		found;

				hentry = hash_search(table_toast_map,
									 &classForm->reltoastrelid,
									 HASH_ENTER, &found);

				if (!found)
				{
					/* hash_search already filled in the key */
					hentry->ar_relid = relid;
					hentry->ar_hasrelopts = false;
					if (relopts != NULL)
					{
						hentry->ar_hasrelopts = true;
						memcpy(&hentry->ar_reloptions, relopts,
							   sizeof(AutoVacOpts));
					}
				}
			}
		}
	}

	heap_endscan(relScan);

	/* second pass: check TOAST tables */
	ScanKeyInit(&key,
				Anum_pg_class_relkind,
				BTEqualStrategyNumber, F_CHAREQ,
				CharGetDatum(RELKIND_TOASTVALUE));

	relScan = heap_beginscan(classRel, SnapshotNow, 1, &key);
	while ((tuple = heap_getnext(relScan, ForwardScanDirection)) != NULL)
	{
		Form_pg_class classForm = (Form_pg_class) GETSTRUCT(tuple);
		PgStat_StatTabEntry *tabentry;
		Oid			relid;
		AutoVacOpts *relopts = NULL;
		bool		dovacuum;
		bool		doanalyze;
		bool		wraparound;

		/*
		 * We cannot safely process other backends' temp tables, so skip 'em.
		 */
		if (classForm->relpersistence == RELPERSISTENCE_TEMP)
			continue;

		relid = HeapTupleGetOid(tuple);

		/*
		 * fetch reloptions -- if this toast table does not have them, try the
		 * main rel
		 */
		relopts = extract_autovac_opts(tuple, pg_class_desc);
		if (relopts == NULL)
		{
			av_relation *hentry;
			bool		found;

			hentry = hash_search(table_toast_map, &relid, HASH_FIND, &found);
			if (found && hentry->ar_hasrelopts)
				relopts = &hentry->ar_reloptions;
		}

		/* Fetch the pgstat entry for this table */
		tabentry = get_pgstat_tabentry_relid(relid, classForm->relisshared,
											 shared, dbentry);

		relation_needs_vacanalyze(relid, relopts, classForm, tabentry,
								  &dovacuum, &doanalyze, &wraparound);

		/* ignore analyze for toast tables */
		if (dovacuum)
			table_oids = lappend_oid(table_oids, relid);
	}

	heap_endscan(relScan);
	heap_close(classRel, AccessShareLock);

	/*
	 * Create a buffer access strategy object for VACUUM to use.  We want to
	 * use the same one across all the vacuum operations we perform, since the
	 * point is for VACUUM not to blow out the shared cache.
	 */
	bstrategy = GetAccessStrategy(BAS_VACUUM);

	/*
	 * create a memory context to act as fake PortalContext, so that the
	 * contexts created in the vacuum code are cleaned up for each table.
	 */
	PortalContext = AllocSetContextCreate(AutovacMemCxt,
										  "Autovacuum Portal",
										  ALLOCSET_DEFAULT_INITSIZE,
										  ALLOCSET_DEFAULT_MINSIZE,
										  ALLOCSET_DEFAULT_MAXSIZE);

	/*
	 * Perform operations on collected tables.
	 */
	foreach(cell, table_oids)
	{
		Oid			relid = lfirst_oid(cell);
		autovac_table *tab;
		bool		skipit;
		int			stdVacuumCostDelay;
		int			stdVacuumCostLimit;
		dlist_iter	iter;

		CHECK_FOR_INTERRUPTS();

		/*
		 * hold schedule lock from here until we're sure that this table still
		 * needs vacuuming.  We also need the AutovacuumLock to walk the
		 * worker array, but we'll let go of that one quickly.
		 */
		LWLockAcquire(AutovacuumScheduleLock, LW_EXCLUSIVE);
		LWLockAcquire(AutovacuumLock, LW_SHARED);

		/*
		 * Check whether the table is being vacuumed concurrently by another
		 * worker.
		 */
		skipit = false;
		dlist_foreach(iter, &AutoVacuumShmem->av_runningWorkers)
		{
			WorkerInfo	worker = dlist_container(WorkerInfoData, wi_links, iter.cur);

			/* ignore myself */
			if (worker == MyWorkerInfo)
				continue;

			/* ignore workers in other databases */
			if (worker->wi_dboid != MyDatabaseId)
				continue;

			if (worker->wi_tableoid == relid)
			{
				skipit = true;
				break;
			}
		}
		LWLockRelease(AutovacuumLock);
		if (skipit)
		{
			LWLockRelease(AutovacuumScheduleLock);
			continue;
		}

		/*
		 * Check whether pgstat data still says we need to vacuum this table.
		 * It could have changed if something else processed the table while
		 * we weren't looking.
		 *
		 * Note: we have a special case in pgstat code to ensure that the
		 * stats we read are as up-to-date as possible, to avoid the problem
		 * that somebody just finished vacuuming this table.  The window to
		 * the race condition is not closed but it is very small.
		 */
		MemoryContextSwitchTo(AutovacMemCxt);
		tab = table_recheck_autovac(relid, table_toast_map, pg_class_desc);
		if (tab == NULL)
		{
			/* someone else vacuumed the table, or it went away */
			LWLockRelease(AutovacuumScheduleLock);
			continue;
		}

		/*
		 * Ok, good to go.	Store the table in shared memory before releasing
		 * the lock so that other workers don't vacuum it concurrently.
		 */
		MyWorkerInfo->wi_tableoid = relid;
		LWLockRelease(AutovacuumScheduleLock);

		/*
		 * Remember the prevailing values of the vacuum cost GUCs.	We have to
		 * restore these at the bottom of the loop, else we'll compute wrong
		 * values in the next iteration of autovac_balance_cost().
		 */
		stdVacuumCostDelay = VacuumCostDelay;
		stdVacuumCostLimit = VacuumCostLimit;

		/* Must hold AutovacuumLock while mucking with cost balance info */
		LWLockAcquire(AutovacuumLock, LW_EXCLUSIVE);

		/* advertise my cost delay parameters for the balancing algorithm */
		MyWorkerInfo->wi_cost_delay = tab->at_vacuum_cost_delay;
		MyWorkerInfo->wi_cost_limit = tab->at_vacuum_cost_limit;
		MyWorkerInfo->wi_cost_limit_base = tab->at_vacuum_cost_limit;

		/* do a balance */
		autovac_balance_cost();

		/* set the active cost parameters from the result of that */
		AutoVacuumUpdateDelay();

		/* done */
		LWLockRelease(AutovacuumLock);

		/* clean up memory before each iteration */
		MemoryContextResetAndDeleteChildren(PortalContext);

		/*
		 * Save the relation name for a possible error message, to avoid a
		 * catalog lookup in case of an error.	If any of these return NULL,
		 * then the relation has been dropped since last we checked; skip it.
		 * Note: they must live in a long-lived memory context because we call
		 * vacuum and analyze in different transactions.
		 */

		tab->at_relname = get_rel_name(tab->at_relid);
		tab->at_nspname = get_namespace_name(get_rel_namespace(tab->at_relid));
		tab->at_datname = get_database_name(MyDatabaseId);
		if (!tab->at_relname || !tab->at_nspname || !tab->at_datname)
			goto deleted;

		/*
		 * We will abort vacuuming the current table if something errors out,
		 * and continue with the next one in schedule; in particular, this
		 * happens if we are interrupted with SIGINT.
		 */
		PG_TRY();
		{
			/* have at it */
			MemoryContextSwitchTo(TopTransactionContext);
			autovacuum_do_vac_analyze(tab, bstrategy);

			/*
			 * Clear a possible query-cancel signal, to avoid a late reaction
			 * to an automatically-sent signal because of vacuuming the
			 * current table (we're done with it, so it would make no sense to
			 * cancel at this point.)
			 */
			QueryCancelPending = false;
		}
		PG_CATCH();
		{
			/*
			 * Abort the transaction, start a new one, and proceed with the
			 * next table in our list.
			 */
			HOLD_INTERRUPTS();
			if (tab->at_dovacuum)
				errcontext("automatic vacuum of table \"%s.%s.%s\"",
						   tab->at_datname, tab->at_nspname, tab->at_relname);
			else
				errcontext("automatic analyze of table \"%s.%s.%s\"",
						   tab->at_datname, tab->at_nspname, tab->at_relname);
			EmitErrorReport();

			/* this resets the PGXACT flags too */
			AbortOutOfAnyTransaction();
			FlushErrorState();
			MemoryContextResetAndDeleteChildren(PortalContext);

			/* restart our transaction for the following operations */
			StartTransactionCommand();
			RESUME_INTERRUPTS();
		}
		PG_END_TRY();

		/* the PGXACT flags are reset at the next end of transaction */

deleted:
		/* be tidy */
		if (tab->at_datname != NULL)
			pfree(tab->at_datname);
		if (tab->at_nspname != NULL)
			pfree(tab->at_nspname);
		if (tab->at_relname != NULL)
			pfree(tab->at_relname);
		pfree(tab);

		/*
		 * Remove my info from shared memory.  We could, but intentionally
		 * don't, clear wi_cost_limit and friends --- this is on the
		 * assumption that we probably have more to do with similar cost
		 * settings, so we don't want to give up our share of I/O for a very
		 * short interval and thereby thrash the global balance.
		 */
		LWLockAcquire(AutovacuumLock, LW_EXCLUSIVE);
		MyWorkerInfo->wi_tableoid = InvalidOid;
		LWLockRelease(AutovacuumLock);

		/* restore vacuum cost GUCs for the next iteration */
		VacuumCostDelay = stdVacuumCostDelay;
		VacuumCostLimit = stdVacuumCostLimit;
	}

	/*
	 * We leak table_toast_map here (among other things), but since we're
	 * going away soon, it's not a problem.
	 */

	/*
	 * Update pg_database.datfrozenxid, and truncate pg_clog if possible. We
	 * only need to do this once, not after each table.
	 */
	vac_update_datfrozenxid();

	/* Finally close out the last transaction. */
	CommitTransactionCommand();
}

/*
 * extract_autovac_opts
 *
 * Given a relation's pg_class tuple, return the AutoVacOpts portion of
 * reloptions, if set; otherwise, return NULL.
 */
static AutoVacOpts *
extract_autovac_opts(HeapTuple tup, TupleDesc pg_class_desc)
{
	bytea	   *relopts;
	AutoVacOpts *av;

	Assert(((Form_pg_class) GETSTRUCT(tup))->relkind == RELKIND_RELATION ||
		   ((Form_pg_class) GETSTRUCT(tup))->relkind == RELKIND_MATVIEW ||
		   ((Form_pg_class) GETSTRUCT(tup))->relkind == RELKIND_TOASTVALUE);

	relopts = extractRelOptions(tup, pg_class_desc, InvalidOid);
	if (relopts == NULL)
		return NULL;

	av = palloc(sizeof(AutoVacOpts));
	memcpy(av, &(((StdRdOptions *) relopts)->autovacuum), sizeof(AutoVacOpts));
	pfree(relopts);

	return av;
}

/*
 * get_pgstat_tabentry_relid
 *
 * Fetch the pgstat entry of a table, either local to a database or shared.
 */
static PgStat_StatTabEntry *
get_pgstat_tabentry_relid(Oid relid, bool isshared, PgStat_StatDBEntry *shared,
						  PgStat_StatDBEntry *dbentry)
{
	PgStat_StatTabEntry *tabentry = NULL;

	if (isshared)
	{
		if (PointerIsValid(shared))
			tabentry = hash_search(shared->tables, &relid,
								   HASH_FIND, NULL);
	}
	else if (PointerIsValid(dbentry))
		tabentry = hash_search(dbentry->tables, &relid,
							   HASH_FIND, NULL);

	return tabentry;
}

/*
 * table_recheck_autovac
 *
 * Recheck whether a table still needs vacuum or analyze.  Return value is a
 * valid autovac_table pointer if it does, NULL otherwise.
 *
 * Note that the returned autovac_table does not have the name fields set.
 */
static autovac_table *
table_recheck_autovac(Oid relid, HTAB *table_toast_map,
					  TupleDesc pg_class_desc)
{
	Form_pg_class classForm;
	HeapTuple	classTup;
	bool		dovacuum;
	bool		doanalyze;
	autovac_table *tab = NULL;
	PgStat_StatTabEntry *tabentry;
	PgStat_StatDBEntry *shared;
	PgStat_StatDBEntry *dbentry;
	bool		wraparound;
	AutoVacOpts *avopts;

	/* use fresh stats */
	autovac_refresh_stats();

	shared = pgstat_fetch_stat_dbentry(InvalidOid);
	dbentry = pgstat_fetch_stat_dbentry(MyDatabaseId);

	/* fetch the relation's relcache entry */
	classTup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relid));
	if (!HeapTupleIsValid(classTup))
		return NULL;
	classForm = (Form_pg_class) GETSTRUCT(classTup);

	/*
	 * Get the applicable reloptions.  If it is a TOAST table, try to get the
	 * main table reloptions if the toast table itself doesn't have.
	 */
	avopts = extract_autovac_opts(classTup, pg_class_desc);
	if (classForm->relkind == RELKIND_TOASTVALUE &&
		avopts == NULL && table_toast_map != NULL)
	{
		av_relation *hentry;
		bool		found;

		hentry = hash_search(table_toast_map, &relid, HASH_FIND, &found);
		if (found && hentry->ar_hasrelopts)
			avopts = &hentry->ar_reloptions;
	}

	/* fetch the pgstat table entry */
	tabentry = get_pgstat_tabentry_relid(relid, classForm->relisshared,
										 shared, dbentry);

	relation_needs_vacanalyze(relid, avopts, classForm, tabentry,
							  &dovacuum, &doanalyze, &wraparound);

	/* ignore ANALYZE for toast tables */
	if (classForm->relkind == RELKIND_TOASTVALUE)
		doanalyze = false;

	/* OK, it needs something done */
	if (doanalyze || dovacuum)
	{
		int			freeze_min_age;
		int			freeze_table_age;
		int			vac_cost_limit;
		int			vac_cost_delay;

		/*
		 * Calculate the vacuum cost parameters and the freeze ages.  If there
		 * are options set in pg_class.reloptions, use them; in the case of a
		 * toast table, try the main table too.  Otherwise use the GUC
		 * defaults, autovacuum's own first and plain vacuum second.
		 */

		/* -1 in autovac setting means use plain vacuum_cost_delay */
		vac_cost_delay = (avopts && avopts->vacuum_cost_delay >= 0)
			? avopts->vacuum_cost_delay
			: (autovacuum_vac_cost_delay >= 0)
			? autovacuum_vac_cost_delay
			: VacuumCostDelay;

		/* 0 or -1 in autovac setting means use plain vacuum_cost_limit */
		vac_cost_limit = (avopts && avopts->vacuum_cost_limit > 0)
			? avopts->vacuum_cost_limit
			: (autovacuum_vac_cost_limit > 0)
			? autovacuum_vac_cost_limit
			: VacuumCostLimit;

		/* these do not have autovacuum-specific settings */
		freeze_min_age = (avopts && avopts->freeze_min_age >= 0)
			? avopts->freeze_min_age
			: default_freeze_min_age;

		freeze_table_age = (avopts && avopts->freeze_table_age >= 0)
			? avopts->freeze_table_age
			: default_freeze_table_age;

		tab = palloc(sizeof(autovac_table));
		tab->at_relid = relid;
		tab->at_dovacuum = dovacuum;
		tab->at_doanalyze = doanalyze;
		tab->at_freeze_min_age = freeze_min_age;
		tab->at_freeze_table_age = freeze_table_age;
		tab->at_vacuum_cost_limit = vac_cost_limit;
		tab->at_vacuum_cost_delay = vac_cost_delay;
		tab->at_wraparound = wraparound;
		tab->at_relname = NULL;
		tab->at_nspname = NULL;
		tab->at_datname = NULL;
	}

	heap_freetuple(classTup);

	return tab;
}

/*
 * relation_needs_vacanalyze
 *
 * Check whether a relation needs to be vacuumed or analyzed; return each into
 * "dovacuum" and "doanalyze", respectively.  Also return whether the vacuum is
 * being forced because of Xid wraparound.
 *
 * relopts is a pointer to the AutoVacOpts options (either for itself in the
 * case of a plain table, or for either itself or its parent table in the case
 * of a TOAST table), NULL if none; tabentry is the pgstats entry, which can be
 * NULL.
 *
 * A table needs to be vacuumed if the number of dead tuples exceeds a
 * threshold.  This threshold is calculated as
 *
 * threshold = vac_base_thresh + vac_scale_factor * reltuples
 *
 * For analyze, the analysis done is that the number of tuples inserted,
 * deleted and updated since the last analyze exceeds a threshold calculated
 * in the same fashion as above.  Note that the collector actually stores
 * the number of tuples (both live and dead) that there were as of the last
 * analyze.  This is asymmetric to the VACUUM case.
 *
 * We also force vacuum if the table's relfrozenxid is more than freeze_max_age
 * transactions back.
 *
 * A table whose autovacuum_enabled option is false is
 * automatically skipped (unless we have to vacuum it due to freeze_max_age).
 * Thus autovacuum can be disabled for specific tables. Also, when the stats
 * collector does not have data about a table, it will be skipped.
 *
 * A table whose vac_base_thresh value is < 0 takes the base value from the
 * autovacuum_vacuum_threshold GUC variable.  Similarly, a vac_scale_factor
 * value < 0 is substituted with the value of
 * autovacuum_vacuum_scale_factor GUC variable.  Ditto for analyze.
 */
static void
relation_needs_vacanalyze(Oid relid,
						  AutoVacOpts *relopts,
						  Form_pg_class classForm,
						  PgStat_StatTabEntry *tabentry,
 /* output params below */
						  bool *dovacuum,
						  bool *doanalyze,
						  bool *wraparound)
{
	bool		force_vacuum;
	bool		av_enabled;
	float4		reltuples;		/* pg_class.reltuples */

	/* constants from reloptions or GUC variables */
	int			vac_base_thresh,
				anl_base_thresh;
	float4		vac_scale_factor,
				anl_scale_factor;

	/* thresholds calculated from above constants */
	float4		vacthresh,
				anlthresh;

	/* number of vacuum (resp. analyze) tuples at this time */
	float4		vactuples,
				anltuples;

	/* freeze parameters */
	int			freeze_max_age;
	TransactionId xidForceLimit;
	MultiXactId multiForceLimit;

	AssertArg(classForm != NULL);
	AssertArg(OidIsValid(relid));

	/*
	 * Determine vacuum/analyze equation parameters.  We have two possible
	 * sources: the passed reloptions (which could be a main table or a toast
	 * table), or the autovacuum GUC variables.
	 */

	/* -1 in autovac setting means use plain vacuum_cost_delay */
	vac_scale_factor = (relopts && relopts->vacuum_scale_factor >= 0)
		? relopts->vacuum_scale_factor
		: autovacuum_vac_scale;

	vac_base_thresh = (relopts && relopts->vacuum_threshold >= 0)
		? relopts->vacuum_threshold
		: autovacuum_vac_thresh;

	anl_scale_factor = (relopts && relopts->analyze_scale_factor >= 0)
		? relopts->analyze_scale_factor
		: autovacuum_anl_scale;

	anl_base_thresh = (relopts && relopts->analyze_threshold >= 0)
		? relopts->analyze_threshold
		: autovacuum_anl_thresh;

	freeze_max_age = (relopts && relopts->freeze_max_age >= 0)
		? Min(relopts->freeze_max_age, autovacuum_freeze_max_age)
		: autovacuum_freeze_max_age;

	av_enabled = (relopts ? relopts->enabled : true);

	/* Force vacuum if table is at risk of wraparound */
	xidForceLimit = recentXid - freeze_max_age;
	if (xidForceLimit < FirstNormalTransactionId)
		xidForceLimit -= FirstNormalTransactionId;
	force_vacuum = (TransactionIdIsNormal(classForm->relfrozenxid) &&
					TransactionIdPrecedes(classForm->relfrozenxid,
										  xidForceLimit));
	if (!force_vacuum)
	{
		multiForceLimit = recentMulti - autovacuum_freeze_max_age;
		if (multiForceLimit < FirstMultiXactId)
			multiForceLimit -= FirstMultiXactId;
		force_vacuum = MultiXactIdPrecedes(classForm->relminmxid,
										   multiForceLimit);
	}
	*wraparound = force_vacuum;

	/* User disabled it in pg_class.reloptions?  (But ignore if at risk) */
	if (!force_vacuum && !av_enabled)
	{
		*doanalyze = false;
		*dovacuum = false;
		return;
	}

	if (PointerIsValid(tabentry))
	{
		reltuples = classForm->reltuples;
		vactuples = tabentry->n_dead_tuples;
		anltuples = tabentry->changes_since_analyze;

		vacthresh = (float4) vac_base_thresh + vac_scale_factor * reltuples;
		anlthresh = (float4) anl_base_thresh + anl_scale_factor * reltuples;

		/*
		 * Note that we don't need to take special consideration for stat
		 * reset, because if that happens, the last vacuum and analyze counts
		 * will be reset too.
		 */
		elog(DEBUG3, "%s: vac: %.0f (threshold %.0f), anl: %.0f (threshold %.0f)",
			 NameStr(classForm->relname),
			 vactuples, vacthresh, anltuples, anlthresh);

		/* Determine if this table needs vacuum or analyze. */
		*dovacuum = force_vacuum || (vactuples > vacthresh);
		*doanalyze = (anltuples > anlthresh);
	}
	else
	{
		/*
		 * Skip a table not found in stat hash, unless we have to force vacuum
		 * for anti-wrap purposes.	If it's not acted upon, there's no need to
		 * vacuum it.
		 */
		*dovacuum = force_vacuum;
		*doanalyze = false;
	}

	/* ANALYZE refuses to work with pg_statistics */
	if (relid == StatisticRelationId)
		*doanalyze = false;
}

/*
 * autovacuum_do_vac_analyze
 *		Vacuum and/or analyze the specified table
 */
static void
autovacuum_do_vac_analyze(autovac_table *tab,
						  BufferAccessStrategy bstrategy)
{
	VacuumStmt	vacstmt;
	RangeVar	rangevar;

	/* Set up command parameters --- use local variables instead of palloc */
	MemSet(&vacstmt, 0, sizeof(vacstmt));
	MemSet(&rangevar, 0, sizeof(rangevar));

	/*
	 * In GPDB, vacuumStatement_Relation() is called to vacuum relation,
	 * which will copy VacuumStmt as its first operation.
	 * We need a valid NodeTag to make copyObject() work correctly.
	 */
	rangevar.type = T_RangeVar;

	rangevar.schemaname = tab->at_nspname;
	rangevar.relname = tab->at_relname;
	rangevar.location = -1;

	vacstmt.type = T_VacuumStmt;
	if (!tab->at_wraparound)
		vacstmt.options = VACOPT_NOWAIT;
	if (tab->at_dovacuum)
		vacstmt.options |= VACOPT_VACUUM;
	if (tab->at_doanalyze)
		vacstmt.options |= VACOPT_ANALYZE;
	vacstmt.freeze_min_age = tab->at_freeze_min_age;
	vacstmt.freeze_table_age = tab->at_freeze_table_age;
	/* we pass the OID, but might need this anyway for an error message */
	vacstmt.relation = &rangevar;
	vacstmt.va_cols = NIL;

	/* Let pgstat know what we're doing */
	autovac_report_activity(tab);

	vacuum(&vacstmt, tab->at_relid, false, bstrategy, tab->at_wraparound, true);
}

/*
 * autovac_report_activity
 *		Report to pgstat what autovacuum is doing
 *
 * We send a SQL string corresponding to what the user would see if the
 * equivalent command was to be issued manually.
 *
 * Note we assume that we are going to report the next command as soon as we're
 * done with the current one, and exit right after the last one, so we don't
 * bother to report "<IDLE>" or some such.
 */
static void
autovac_report_activity(autovac_table *tab)
{
#define MAX_AUTOVAC_ACTIV_LEN (NAMEDATALEN * 2 + 56)
	char		activity[MAX_AUTOVAC_ACTIV_LEN];
	int			len;

	/* Report the command and possible options */
	if (tab->at_dovacuum)
		snprintf(activity, MAX_AUTOVAC_ACTIV_LEN,
				 "autovacuum: VACUUM%s",
				 tab->at_doanalyze ? " ANALYZE" : "");
	else
		snprintf(activity, MAX_AUTOVAC_ACTIV_LEN,
				 "autovacuum: ANALYZE");

	/*
	 * Report the qualified name of the relation.
	 */
	len = strlen(activity);

	snprintf(activity + len, MAX_AUTOVAC_ACTIV_LEN - len,
			 " %s.%s%s", tab->at_nspname, tab->at_relname,
			 tab->at_wraparound ? " (to prevent wraparound)" : "");

	/* Set statement_timestamp() to current time for pg_stat_activity */
	SetCurrentStatementStartTimestamp();

	pgstat_report_activity(STATE_RUNNING, activity);
}

/*
 * AutoVacuumingActive
 *		Check GUC vars and report whether the autovacuum process should be
 *		running.
 */
bool
AutoVacuumingActive(void)
{
	if (!autovacuum_start_daemon || !pgstat_track_counts)
		return false;
	return true;
}

/*
 * autovac_init
 *		This is called at postmaster initialization.
 *
 * All we do here is annoy the user if he got it wrong.
 */
void
autovac_init(void)
{
	if (autovacuum_start_daemon && !pgstat_track_counts)
		ereport(WARNING,
				(errmsg("autovacuum not started because of misconfiguration"),
				 errhint("Enable the \"track_counts\" option.")));
}

/*
 * IsAutoVacuum functions
 *		Return whether this is either a launcher autovacuum process or a worker
 *		process.
 */
bool
IsAutoVacuumLauncherProcess(void)
{
	return am_autovacuum_launcher;
}

bool
IsAutoVacuumWorkerProcess(void)
{
	return am_autovacuum_worker;
}


/*
 * AutoVacuumShmemSize
 *		Compute space needed for autovacuum-related shared memory
 */
Size
AutoVacuumShmemSize(void)
{
	Size		size;

	/*
	 * Need the fixed struct and the array of WorkerInfoData.
	 */
	size = sizeof(AutoVacuumShmemStruct);
	size = MAXALIGN(size);
	size = add_size(size, mul_size(autovacuum_max_workers,
								   sizeof(WorkerInfoData)));
	return size;
}

/*
 * AutoVacuumShmemInit
 *		Allocate and initialize autovacuum-related shared memory
 */
void
AutoVacuumShmemInit(void)
{
	bool		found;

	AutoVacuumShmem = (AutoVacuumShmemStruct *)
		ShmemInitStruct("AutoVacuum Data",
						AutoVacuumShmemSize(),
						&found);

	if (!IsUnderPostmaster)
	{
		WorkerInfo	worker;
		int			i;

		Assert(!found);

		AutoVacuumShmem->av_launcherpid = 0;
		dlist_init(&AutoVacuumShmem->av_freeWorkers);
		dlist_init(&AutoVacuumShmem->av_runningWorkers);
		AutoVacuumShmem->av_startingWorker = NULL;

		worker = (WorkerInfo) ((char *) AutoVacuumShmem +
							   MAXALIGN(sizeof(AutoVacuumShmemStruct)));

		/* initialize the WorkerInfo free list */
		for (i = 0; i < autovacuum_max_workers; i++)
			dlist_push_head(&AutoVacuumShmem->av_freeWorkers,
							&worker[i].wi_links);
	}
	else
		Assert(found);
}

/*
 * autovac_refresh_stats
 *		Refresh pgstats data for an autovacuum process
 *
 * Cause the next pgstats read operation to obtain fresh data, but throttle
 * such refreshing in the autovacuum launcher.	This is mostly to avoid
 * rereading the pgstats files too many times in quick succession when there
 * are many databases.
 *
 * Note: we avoid throttling in the autovac worker, as it would be
 * counterproductive in the recheck logic.
 */
static void
autovac_refresh_stats(void)
{
	if (IsAutoVacuumLauncherProcess())
	{
		static TimestampTz last_read = 0;
		TimestampTz current_time;

		current_time = GetCurrentTimestamp();

		if (!TimestampDifferenceExceeds(last_read, current_time,
										STATS_READ_DELAY))
			return;

		last_read = current_time;
	}

	pgstat_clear_snapshot();
}

/*
 * In GPDB, autovacuum is currently disabled, except for the
 * anti-wraparound vacuum of template0 (and any other databases
 * with !datallowconn). The administrator is expected to do all
 * VACUUMing manually, except for template0, which you cannot
 * VACUUM manually because you cannot connect to it.
 */
bool 
GpAutovacuumEnabled(Form_pg_database dbform) 
{
	return !dbform->datallowconn;
}

/*
 * autovacuum when the current transaction id is older than the
 * autovacuum_freeze_max_age iff the database does not
 * allow connections.
 */
bool
GpDatabaseNeedsAutovacuum(Form_pg_database dbform, TransactionId xid)
{
	TransactionId xidVacLimit;
	
	if (!GpAutovacuumEnabled(dbform))
		return false;

	xidVacLimit = dbform->datfrozenxid + autovacuum_freeze_max_age;
	
	return TransactionIdPrecedesOrEquals(xidVacLimit, xid);
}
