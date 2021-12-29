/*-------------------------------------------------------------------------
 *
 * pgarch.c
 *
 *	PostgreSQL WAL archiver
 *
 *	All functions relating to archiver are included here
 *
 *	- All functions executed by archiver process
 *
 *	- archiver is forked from postmaster, and the two
 *	processes then communicate using signals. All functions
 *	executed by postmaster are included in this file.
 *
 *	Initial author: Simon Riggs		simon@2ndquadrant.com
 *
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/postmaster/pgarch.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>

#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "lib/binaryheap.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/fork_process.h"
#include "postmaster/pgarch.h"
#include "postmaster/postmaster.h"
#include "storage/dsm.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/pg_shmem.h"
#include "storage/pmsignal.h"
#include "utils/guc.h"
#include "utils/ps_status.h"

/*
 * GPDB specific imports:
 */
#include "cdb/cdbvars.h"
#include "utils/builtins.h"

/* ----------
 * Timer definitions.
 * ----------
 */
#define PGARCH_AUTOWAKE_INTERVAL 60		/* How often to force a poll of the
										 * archive status directory; in
										 * seconds. */
#define PGARCH_RESTART_INTERVAL 10		/* How often to attempt to restart a
										 * failed archiver; in seconds. */

#define NUM_ARCHIVE_RETRIES 3

/*
 * Maximum number of .ready files to gather per directory scan.
 */
#define NUM_FILES_PER_DIRECTORY_SCAN 64

/* ----------
 * Local data
 * ----------
 */
static time_t last_pgarch_start_time;
static time_t last_sigterm_time = 0;

/*
 * Stuff for tracking multiple files to archive from each scan of
 * archive_status.  Minimizing the number of directory scans when there are
 * many files to archive can significantly improve archival rate.
 *
 * arch_heap is a max-heap that is used during the directory scan to track
 * the highest-priority files to archive.  After the directory scan
 * completes, the file names are stored in ascending order of priority in
 * arch_files.  pgarch_readyXlog() returns files from arch_files until it
 * is empty, at which point another directory scan must be performed.
 *
 * We only need this data in the archiver process, so make it a palloc'd
 * struct rather than a bunch of static arrays.
 */
struct arch_files_state
{
	binaryheap *arch_heap;
	int			arch_files_size;	/* number of live entries in arch_files[] */
	char	   *arch_files[NUM_FILES_PER_DIRECTORY_SCAN];
	/* buffers underlying heap, and later arch_files[], entries: */
	char		arch_filenames[NUM_FILES_PER_DIRECTORY_SCAN][MAX_XFN_CHARS + 1];
};

static struct arch_files_state *arch_files = NULL;

/*
 * Flags set by interrupt handlers for later service in the main loop.
 */
static volatile sig_atomic_t got_SIGHUP = false;
static volatile sig_atomic_t got_SIGTERM = false;
static volatile sig_atomic_t wakened = false;
static volatile sig_atomic_t ready_to_stop = false;

/*
 * Latch used by signal handlers to wake up the sleep in the main loop.
 */
static Latch mainloop_latch;

/* ----------
 * Local function forward declarations
 * ----------
 */
#ifdef EXEC_BACKEND
static pid_t pgarch_forkexec(void);
#endif

NON_EXEC_STATIC void PgArchiverMain(int argc, char *argv[]) __attribute__((noreturn));
static void pgarch_exit(SIGNAL_ARGS);
static void ArchSigHupHandler(SIGNAL_ARGS);
static void ArchSigTermHandler(SIGNAL_ARGS);
static void pgarch_waken(SIGNAL_ARGS);
static void pgarch_waken_stop(SIGNAL_ARGS);
static void pgarch_MainLoop(void);
static void pgarch_ArchiverCopyLoop(void);
static bool pgarch_archiveXlog(char *xlog);
static bool pgarch_readyXlog(char *xlog);
static void pgarch_archiveDone(char *xlog);
static int ready_file_comparator(Datum a, Datum b, void *arg);

/* ------------------------------------------------------------
 * Public functions called from postmaster follow
 * ------------------------------------------------------------
 */

/*
 * pgarch_start
 *
 *	Called from postmaster at startup or after an existing archiver
 *	died.  Attempt to fire up a fresh archiver process.
 *
 *	Returns PID of child process, or 0 if fail.
 *
 *	Note: if fail, we will be called again from the postmaster main loop.
 */
int
pgarch_start(void)
{
	time_t		curtime;
	pid_t		pgArchPid;

	/*
	 * Do nothing if no archiver needed
	 */
	if (!XLogArchivingActive())
		return 0;

	/*
	 * Do nothing if too soon since last archiver start.  This is a safety
	 * valve to protect against continuous respawn attempts if the archiver is
	 * dying immediately at launch. Note that since we will be re-called from
	 * the postmaster main loop, we will get another chance later.
	 */
	curtime = time(NULL);
	if ((unsigned int) (curtime - last_pgarch_start_time) <
		(unsigned int) PGARCH_RESTART_INTERVAL)
		return 0;
	last_pgarch_start_time = curtime;

#ifdef EXEC_BACKEND
	switch ((pgArchPid = pgarch_forkexec()))
#else
	switch ((pgArchPid = fork_process()))
#endif
	{
		case -1:
			ereport(LOG,
					(errmsg("could not fork archiver: %m")));
			return 0;

#ifndef EXEC_BACKEND
		case 0:
			/* in postmaster child ... */
			/* Close the postmaster's sockets */
			ClosePostmasterPorts(false);

			/* Lose the postmaster's on-exit routines */
			on_exit_reset();

			/* Drop our connection to postmaster's shared memory, as well */
			dsm_detach_all();
			PGSharedMemoryDetach();

			PgArchiverMain(0, NULL);
			break;
#endif

		default:
			return (int) pgArchPid;
	}

	/* shouldn't get here */
	return 0;
}

/* ------------------------------------------------------------
 * Local functions called by archiver follow
 * ------------------------------------------------------------
 */


#ifdef EXEC_BACKEND

/*
 * pgarch_forkexec() -
 *
 * Format up the arglist for, then fork and exec, archive process
 */
static pid_t
pgarch_forkexec(void)
{
	char	   *av[10];
	int			ac = 0;

	av[ac++] = "postgres";

	av[ac++] = "--forkarch";

	av[ac++] = NULL;			/* filled in by postmaster_forkexec */

	av[ac] = NULL;
	Assert(ac < lengthof(av));

	return postmaster_forkexec(ac, av);
}
#endif   /* EXEC_BACKEND */


/*
 * PgArchiverMain
 *
 *	The argc/argv parameters are valid only in EXEC_BACKEND case.  However,
 *	since we don't use 'em, it hardly matters...
 */
NON_EXEC_STATIC void
PgArchiverMain(int argc, char *argv[])
{
	IsUnderPostmaster = true;	/* we are a postmaster subprocess now */

	MyProcPid = getpid();		/* reset MyProcPid */

	MyStartTime = time(NULL);	/* record Start Time for logging */

	/*
	 * If possible, make this process a group leader, so that the postmaster
	 * can signal any child processes too.
	 */
#ifdef HAVE_SETSID
	if (setsid() < 0)
		elog(FATAL, "setsid() failed: %m");
#endif

	InitializeLatchSupport();	/* needed for latch waits */

	InitLatch(&mainloop_latch); /* initialize latch used in main loop */

	/*
	 * Ignore all signals usually bound to some action in the postmaster,
	 * except for SIGHUP, SIGTERM, SIGUSR1, SIGUSR2, and SIGQUIT.
	 */
	pqsignal(SIGHUP, ArchSigHupHandler);
	pqsignal(SIGINT, SIG_IGN);
	pqsignal(SIGTERM, ArchSigTermHandler);
	pqsignal(SIGQUIT, pgarch_exit);
	pqsignal(SIGALRM, SIG_IGN);
	pqsignal(SIGPIPE, SIG_IGN);
	pqsignal(SIGUSR1, pgarch_waken);
	pqsignal(SIGUSR2, pgarch_waken_stop);
	pqsignal(SIGCHLD, SIG_DFL);
	pqsignal(SIGTTIN, SIG_DFL);
	pqsignal(SIGTTOU, SIG_DFL);
	pqsignal(SIGCONT, SIG_DFL);
	pqsignal(SIGWINCH, SIG_DFL);
	PG_SETMASK(&UnBlockSig);

	/*
	 * Identify myself via ps
	 */
	init_ps_display("archiver process", "", "", "");

	/* Create workspace for pgarch_readyXlog() */
	arch_files = palloc(sizeof(struct arch_files_state));
	arch_files->arch_files_size = 0;

	/* Initialize our max-heap for prioritizing files to archive. */
	arch_files->arch_heap = binaryheap_allocate(NUM_FILES_PER_DIRECTORY_SCAN,
												ready_file_comparator, NULL);

	pgarch_MainLoop();

	exit(0);
}

/* SIGQUIT signal handler for archiver process */
static void
pgarch_exit(SIGNAL_ARGS)
{
	/* SIGQUIT means curl up and die ... */
	exit(1);
}

/* SIGHUP signal handler for archiver process */
static void
ArchSigHupHandler(SIGNAL_ARGS)
{
	int			save_errno = errno;

	/* set flag to re-read config file at next convenient time */
	got_SIGHUP = true;
	SetLatch(&mainloop_latch);

	errno = save_errno;
}

/* SIGTERM signal handler for archiver process */
static void
ArchSigTermHandler(SIGNAL_ARGS)
{
	int			save_errno = errno;

	/*
	 * The postmaster never sends us SIGTERM, so we assume that this means
	 * that init is trying to shut down the whole system.  If we hang around
	 * too long we'll get SIGKILL'd.  Set flag to prevent starting any more
	 * archive commands.
	 */
	got_SIGTERM = true;
	SetLatch(&mainloop_latch);

	errno = save_errno;
}

/* SIGUSR1 signal handler for archiver process */
static void
pgarch_waken(SIGNAL_ARGS)
{
	int			save_errno = errno;

	/* set flag that there is work to be done */
	wakened = true;
	SetLatch(&mainloop_latch);

	errno = save_errno;
}

/* SIGUSR2 signal handler for archiver process */
static void
pgarch_waken_stop(SIGNAL_ARGS)
{
	int			save_errno = errno;

	/* set flag to do a final cycle and shut down afterwards */
	ready_to_stop = true;
	SetLatch(&mainloop_latch);

	errno = save_errno;
}

/*
 * pgarch_MainLoop
 *
 * Main loop for archiver
 */
static void
pgarch_MainLoop(void)
{
	pg_time_t	last_copy_time = 0;
	bool		time_to_stop;

	/*
	 * We run the copy loop immediately upon entry, in case there are
	 * unarchived files left over from a previous database run (or maybe the
	 * archiver died unexpectedly).  After that we wait for a signal or
	 * timeout before doing more.
	 */
	wakened = true;

	/*
	 * There shouldn't be anything for the archiver to do except to wait for a
	 * signal ... however, the archiver exists to protect our data, so she
	 * wakes up occasionally to allow herself to be proactive.
	 */
	do
	{
		ResetLatch(&mainloop_latch);

		/* When we get SIGUSR2, we do one more archive cycle, then exit */
		time_to_stop = ready_to_stop;

		/* Check for config update */
		if (got_SIGHUP)
		{
			got_SIGHUP = false;
			ProcessConfigFile(PGC_SIGHUP);
		}

		/*
		 * If we've gotten SIGTERM, we normally just sit and do nothing until
		 * SIGUSR2 arrives.  However, that means a random SIGTERM would
		 * disable archiving indefinitely, which doesn't seem like a good
		 * idea.  If more than 60 seconds pass since SIGTERM, exit anyway, so
		 * that the postmaster can start a new archiver if needed.
		 */
		if (got_SIGTERM)
		{
			time_t		curtime = time(NULL);

			if (last_sigterm_time == 0)
				last_sigterm_time = curtime;
			else if ((unsigned int) (curtime - last_sigterm_time) >=
					 (unsigned int) 60)
				break;
		}

		/* Do what we're here for */
		if (wakened || time_to_stop)
		{
			wakened = false;
			pgarch_ArchiverCopyLoop();
			last_copy_time = time(NULL);
		}

		/*
		 * Sleep until a signal is received, or until a poll is forced by
		 * PGARCH_AUTOWAKE_INTERVAL having passed since last_copy_time, or
		 * until postmaster dies.
		 */
		if (!time_to_stop)		/* Don't wait during last iteration */
		{
			pg_time_t	curtime = (pg_time_t) time(NULL);
			int			timeout;

			timeout = PGARCH_AUTOWAKE_INTERVAL - (curtime - last_copy_time);
			if (timeout > 0)
			{
				int			rc;

				rc = WaitLatch(&mainloop_latch,
							 WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
							   timeout * 1000L);
				if (rc & WL_TIMEOUT)
					wakened = true;
			}
			else
				wakened = true;
		}

		/*
		 * The archiver quits either when the postmaster dies (not expected)
		 * or after completing one more archiving cycle after receiving
		 * SIGUSR2.
		 */
	} while (PostmasterIsAlive() && !time_to_stop);
}

/*
 * pgarch_ArchiverCopyLoop
 *
 * Archives all outstanding xlogs then returns
 */
static void
pgarch_ArchiverCopyLoop(void)
{
	char		xlog[MAX_XFN_CHARS + 1];

	/* force directory scan in the first call to pgarch_readyXlog() */
	arch_files->arch_files_size = 0;

	/*
	 * loop through all xlogs with archive_status of .ready and archive
	 * them...mostly we expect this to be a single file, though it is possible
	 * some backend will add files onto the list of those that need archiving
	 * while we are still copying earlier archives
	 */
	while (pgarch_readyXlog(xlog))
	{
		int			failures = 0;

		for (;;)
		{
			/*
			 * Do not initiate any more archive commands after receiving
			 * SIGTERM, nor after the postmaster has died unexpectedly. The
			 * first condition is to try to keep from having init SIGKILL the
			 * command, and the second is to avoid conflicts with another
			 * archiver spawned by a newer postmaster.
			 */
			if (got_SIGTERM || !PostmasterIsAlive())
				return;

			/*
			 * Check for config update.  This is so that we'll adopt a new
			 * setting for archive_command as soon as possible, even if there
			 * is a backlog of files to be archived.
			 */
			if (got_SIGHUP)
			{
				got_SIGHUP = false;
				ProcessConfigFile(PGC_SIGHUP);
			}

			/* can't do anything if no command ... */
			if (!XLogArchiveCommandSet())
			{
				ereport(WARNING,
						(errmsg("archive_mode enabled, yet archive_command is not set")));
				return;
			}

			if (pgarch_archiveXlog(xlog))
			{
				/* successful */
				pgarch_archiveDone(xlog);

				/*
				 * Tell the collector about the WAL file that we successfully
				 * archived
				 */
				pgstat_send_archiver(xlog, false);

				break;			/* out of inner retry loop */
			}
			else
			{
				/*
				 * Tell the collector about the WAL file that we failed to
				 * archive
				 */
				pgstat_send_archiver(xlog, true);

				if (++failures >= NUM_ARCHIVE_RETRIES)
				{
					ereport(WARNING,
							(errmsg("archiving transaction log file \"%s\" failed too many times, will try again later",
									xlog)));
					return;		/* give up archiving for now */
				}
				pg_usleep(1000000L);	/* wait a bit before retrying */
			}
		}
	}
}

/*
 * pgarch_archiveXlog
 *
 * Invokes system(3) to copy one archive file to wherever it should go
 *
 * Returns true if successful
 */
static bool
pgarch_archiveXlog(char *xlog)
{
	char		xlogarchcmd[MAXPGPATH];
	char		pathname[MAXPGPATH];
	char		activitymsg[MAXFNAMELEN + 16];
	char	   *dp;
	char	   *endp;
	const char *sp;
	int			rc;

	char		contentid[12];	/* sign, 10 digits and '\0' */

	snprintf(pathname, MAXPGPATH, XLOGDIR "/%s", xlog);

	/*
	 * construct the command to be executed
	 */
	dp = xlogarchcmd;
	endp = xlogarchcmd + MAXPGPATH - 1;
	*endp = '\0';

	for (sp = XLogArchiveCommand; *sp; sp++)
	{
		if (*sp == '%')
		{
			switch (sp[1])
			{
				case 'p':
					/* %p: relative path of source file */
					sp++;
					strlcpy(dp, pathname, endp - dp);
					make_native_path(dp);
					dp += strlen(dp);
					break;
				case 'f':
					/* %f: filename of source file */
					sp++;
					strlcpy(dp, xlog, endp - dp);
					dp += strlen(dp);
					break;
				case 'c':
					/* GPDB: %c: contentId of segment */
					Assert(GpIdentity.segindex != UNINITIALIZED_GP_IDENTITY_VALUE);
					sp++;
					pg_ltoa(GpIdentity.segindex, contentid);
					strlcpy(dp, contentid, endp - dp);
					dp += strlen(dp);
					break;
				case '%':
					/* convert %% to a single % */
					sp++;
					if (dp < endp)
						*dp++ = *sp;
					break;
				default:
					/* otherwise treat the % as not special */
					if (dp < endp)
						*dp++ = *sp;
					break;
			}
		}
		else
		{
			if (dp < endp)
				*dp++ = *sp;
		}
	}
	*dp = '\0';

	ereport(DEBUG3,
			(errmsg_internal("executing archive command \"%s\"",
							 xlogarchcmd)));

	/* Report archive activity in PS display */
	snprintf(activitymsg, sizeof(activitymsg), "archiving %s", xlog);
	set_ps_display(activitymsg, false);

	rc = system(xlogarchcmd);
	if (rc != 0)
	{
		/*
		 * If either the shell itself, or a called command, died on a signal,
		 * abort the archiver.  We do this because system() ignores SIGINT and
		 * SIGQUIT while waiting; so a signal is very likely something that
		 * should have interrupted us too.  If we overreact it's no big deal,
		 * the postmaster will just start the archiver again.
		 *
		 * Per the Single Unix Spec, shells report exit status > 128 when a
		 * called command died on a signal.
		 */
		int			lev = (WIFSIGNALED(rc) || WEXITSTATUS(rc) > 128) ? FATAL : LOG;

		if (WIFEXITED(rc))
		{
			ereport(lev,
					(errmsg("archive command failed with exit code %d",
							WEXITSTATUS(rc)),
					 errdetail("The failed archive command was: %s",
							   xlogarchcmd)));
		}
		else if (WIFSIGNALED(rc))
		{
#if defined(WIN32)
			ereport(lev,
				  (errmsg("archive command was terminated by exception 0x%X",
						  WTERMSIG(rc)),
				   errhint("See C include file \"ntstatus.h\" for a description of the hexadecimal value."),
				   errdetail("The failed archive command was: %s",
							 xlogarchcmd)));
#elif defined(HAVE_DECL_SYS_SIGLIST) && HAVE_DECL_SYS_SIGLIST
			ereport(lev,
					(errmsg("archive command was terminated by signal %d: %s",
							WTERMSIG(rc),
			  WTERMSIG(rc) < NSIG ? sys_siglist[WTERMSIG(rc)] : "(unknown)"),
					 errdetail("The failed archive command was: %s",
							   xlogarchcmd)));
#else
			ereport(lev,
					(errmsg("archive command was terminated by signal %d",
							WTERMSIG(rc)),
					 errdetail("The failed archive command was: %s",
							   xlogarchcmd)));
#endif
		}
		else
		{
			ereport(lev,
				(errmsg("archive command exited with unrecognized status %d",
						rc),
				 errdetail("The failed archive command was: %s",
						   xlogarchcmd)));
		}

		snprintf(activitymsg, sizeof(activitymsg), "failed on %s", xlog);
		set_ps_display(activitymsg, false);

		return false;
	}
	ereport(DEBUG1,
			(errmsg("archived transaction log file \"%s\"", xlog)));

	snprintf(activitymsg, sizeof(activitymsg), "last was %s", xlog);
	set_ps_display(activitymsg, false);

	return true;
}

/*
 * pgarch_readyXlog
 *
 * Return name of the oldest xlog file that has not yet been archived.
 * No notification is set that file archiving is now in progress, so
 * this would need to be extended if multiple concurrent archival
 * tasks were created. If a failure occurs, we will completely
 * re-copy the file at the next available opportunity.
 *
 * It is important that we return the oldest, so that we archive xlogs
 * in order that they were written, for two reasons:
 * 1) to maintain the sequential chain of xlogs required for recovery
 * 2) because the oldest ones will sooner become candidates for
 * recycling at time of checkpoint
 *
 * NOTE: the "oldest" comparison will consider any .history file to be older
 * than any other file except another .history file.  Segments on a timeline
 * with a smaller ID will be older than all segments on a timeline with a
 * larger ID; the net result being that past timelines are given higher
 * priority for archiving.  This seems okay, or at least not obviously worth
 * changing.
 */
static bool
pgarch_readyXlog(char *xlog)
{
	char		XLogArchiveStatusDir[MAXPGPATH];
	DIR		   *rldir;
	struct dirent *rlde;

	/*
	 * If we still have stored file names from the previous directory scan,
	 * try to return one of those.  We check to make sure the status file
	 * is still present, as the archive_command for a previous file may
	 * have already marked it done.
	 */
	while (arch_files->arch_files_size > 0)
	{
		struct stat	st;
		char		status_file[MAXPGPATH];
		char	   *arch_file;

		arch_files->arch_files_size--;
		arch_file = arch_files->arch_files[arch_files->arch_files_size];
		StatusFilePath(status_file, arch_file, ".ready");

		if (stat(status_file, &st) == 0)
		{
			strcpy(xlog, arch_file);
			return true;
		}
		else if (errno != ENOENT)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not stat file \"%s\": %m", status_file)));
	}

	/* arch_heap is probably empty, but let's make sure */
	binaryheap_reset(arch_files->arch_heap);

	/*
	 * Open the archive status directory and read through the list of files
	 * with the .ready suffix, looking for the earliest files.
	 */
	snprintf(XLogArchiveStatusDir, MAXPGPATH, XLOGDIR "/archive_status");
	rldir = AllocateDir(XLogArchiveStatusDir);
	if (rldir == NULL)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open archive status directory \"%s\": %m",
						XLogArchiveStatusDir)));

	while ((rlde = ReadDir(rldir, XLogArchiveStatusDir)) != NULL)
	{
		int			basenamelen = (int) strlen(rlde->d_name) - 6;
		char		basename[MAX_XFN_CHARS + 1];
		char	   *arch_file;

		/* Ignore entries with unexpected number of characters */
		if (basenamelen < MIN_XFN_CHARS ||
			basenamelen > MAX_XFN_CHARS)
			continue;

		/* Ignore entries with unexpected characters */
		if (strspn(rlde->d_name, VALID_XFN_CHARS) < basenamelen)
			continue;

		/* Ignore anything not suffixed with .ready */
		if (strcmp(rlde->d_name + basenamelen, ".ready") != 0)
			continue;

		/* Truncate off the .ready */
		memcpy(basename, rlde->d_name, basenamelen);
		basename[basenamelen] = '\0';

		/*
		 * Store the file in our max-heap if it has a high enough priority.
		 */
		if (arch_files->arch_heap->bh_size < NUM_FILES_PER_DIRECTORY_SCAN)
		{
			/* If the heap isn't full yet, quickly add it. */
			arch_file = arch_files->arch_filenames[arch_files->arch_heap->bh_size];
			strcpy(arch_file, basename);
			binaryheap_add_unordered(arch_files->arch_heap, CStringGetDatum(arch_file));

			/* If we just filled the heap, make it a valid one. */
			if (arch_files->arch_heap->bh_size == NUM_FILES_PER_DIRECTORY_SCAN)
				binaryheap_build(arch_files->arch_heap);
		}
		else if (ready_file_comparator(binaryheap_first(arch_files->arch_heap),
									   CStringGetDatum(basename), NULL) > 0)
		{
			/*
			 * Remove the lowest priority file and add the current one to
			 * the heap.
			 */
			arch_file = DatumGetCString(binaryheap_remove_first(arch_files->arch_heap));
			strcpy(arch_file, basename);
			binaryheap_add(arch_files->arch_heap, CStringGetDatum(arch_file));
		}
	}
	FreeDir(rldir);

	/* If no files were found, simply return. */
	if (arch_files->arch_heap->bh_size == 0)
		return false;

	/*
	 * If we didn't fill the heap, we didn't make it a valid one.  Do that
	 * now.
	 */
	if (arch_files->arch_heap->bh_size < NUM_FILES_PER_DIRECTORY_SCAN)
		binaryheap_build(arch_files->arch_heap);

	/*
	 * Fill arch_files array with the files to archive in ascending order
	 * of priority.
	 */
	arch_files->arch_files_size = arch_files->arch_heap->bh_size;
	for (int i = 0; i < arch_files->arch_files_size; i++)
		arch_files->arch_files[i] = DatumGetCString(binaryheap_remove_first(arch_files->arch_heap));

	/* Return the highest priority file. */
	arch_files->arch_files_size--;
	strcpy(xlog, arch_files->arch_files[arch_files->arch_files_size]);

	return true;
}

/*
 * ready_file_comparator
 *
 * Compares the archival priority of the given files to archive.  If "a"
 * has a higher priority than "b", a negative value will be returned.  If
 * "b" has a higher priority than "a", a positive value will be returned.
 * If "a" and "b" have equivalent values, 0 will be returned.
 */
static int
ready_file_comparator(Datum a, Datum b, void *arg)
{
	char *a_str = DatumGetCString(a);
	char *b_str = DatumGetCString(b);
	bool a_history = IsTLHistoryFileName(a_str);
	bool b_history = IsTLHistoryFileName(b_str);

	/* Timeline history files always have the highest priority. */
	if (a_history != b_history)
		return a_history ? -1 : 1;

	/* Priority is given to older files. */
	return strcmp(a_str, b_str);
}

/*
 * pgarch_archiveDone
 *
 * Emit notification that an xlog file has been successfully archived.
 * We do this by renaming the status file from NNN.ready to NNN.done.
 * Eventually, a checkpoint process will notice this and delete both the
 * NNN.done file and the xlog file itself.
 */
static void
pgarch_archiveDone(char *xlog)
{
	char		rlogready[MAXPGPATH];
	char		rlogdone[MAXPGPATH];

	StatusFilePath(rlogready, xlog, ".ready");
	StatusFilePath(rlogdone, xlog, ".done");
	(void) durable_rename(rlogready, rlogdone, WARNING);
}
