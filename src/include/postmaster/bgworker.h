/*-------------------------------------------------------------------------
 * bgworker.h
 *		Greenplum Database specific pluggable background workers interface.
 * This is a GPDB specific implementation of background workers, with limited
 * function. Can be replaced by postgres bgworker once merge work comes to PG9.3
 *
 * A background worker is a process able to run arbitrary, user-supplied code.
 *
 * Any external module loaded via shared_preload_libraries can register a
 * worker. The worker process is forked from the postmaster and runs the
 * user-supplied "main" function. Workers can remain active indefinitely,
 * but will be terminated if a shutdown or crash occurs.
 *
 * If the fork() call fails in the postmaster, it will try again later.  Note
 * that the failure can only be transient (fork failure due to high load,
 * memory pressure, too many processes, etc). A worker which exits with
 * a return code of 0 will be restarted immediately, A worker which exits with
 * other return code will be restarted after the configured restart interval
 * (unless that interval is BGW_NEVER_RESTART).
 *
 * Note that there might be more than one worker in a database concurrently,
 * and the same module may request more than one worker running the same (or
 * different) code.
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc.
 *
 * IDENTIFICATION
 *		src/include/postmaster/bgworker.h
 *--------------------------------------------------------------------
 */
#ifndef BGWORKER_H
#define BGWORKER_H

/*
 * Pass this flag to have your worker be able to connect to shared memory.
 */
#define BGWORKER_SHMEM_ACCESS		0x0001
#define BGW_NEVER_RESTART			-1

typedef void (*bgworker_main_type) (void *main_arg);
typedef void (*bgworker_sighdlr_type) (SIGNAL_ARGS);


typedef struct BackgroundWorker
{
	char	   *bgw_name;
	int			bgw_flags;
	int			bgw_restart_time;	/* in seconds, or BGW_NEVER_RESTART */
	bgworker_main_type bgw_main;
	void	   *bgw_main_arg;
	bgworker_sighdlr_type bgw_sighup;
	bgworker_sighdlr_type bgw_sigterm;
} BackgroundWorker;

/*
 * List of background workers.
 *
 * GPDB bgworker specific: Database connection is not supported now
 */
typedef struct RegisteredBgWorker
{
	BackgroundWorker rw_worker; /* its registry entry */
	pid_t		rw_pid;			/* 0 if not running */
	int			rw_child_slot;
	TimestampTz rw_crashed_at;	/* if not 0, time it last crashed */
#ifdef EXEC_BACKEND
	int			rw_cookie;
#endif
} RegisteredBgWorker;

extern List *BackgroundWorkerList;
extern BackgroundWorker *MyBgworkerEntry;

extern bool HaveCrashedWorker;
extern bool StartWorkerNeeded;

extern void StartOneBackgroundWorker(bool fatalError, int, int);
extern void DetermineSleepTime(struct timeval *timeout, int shutdown);
extern bool SignalUnconnectedWorkers(int signal, void (*signal_child) (pid_t, int));
extern bool CleanupBackgroundWorker(int pid, int exitstatus, void (*) (int, int, const char *), void (*) (int, const char *, int, int));
extern void HandleBgworkerCrash(int pid, void (*signal_child) (pid_t, int), bool fatalError, int sendStop);
extern int	CountUnconnectedWorkers(void);
extern void RegisterBackgroundWorker(BackgroundWorker *worker);

#ifdef EXEC_BACKEND
BackgroundWorker *find_bgworker_entry(int cookie);
#endif

extern void RegisterBackgroundWorker(BackgroundWorker * worker);
extern int	GetNumShmemAttachedBgworkers(void);
/* Block/unblock signals in a background worker process */
extern void BackgroundWorkerUnblockSignals(void);
#endif							/* BGWORKER_H */
