/* ------------------------------------------------------------------------
 *
 * bgworker_example.c
 *		Sample implementation of workers for Greenplum Database specific
 *		bgworker framework.
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc.
 *
 * IDENTIFICATION
 *		contrib/bgworker_example/bgworker_example.c
 *
 * ------------------------------------------------------------------------
 */
#include "postgres.h"

/* These are always necessary for a bgworker */
#include "miscadmin.h"
#include "nodes/pg_list.h"
#include "utils/timestamp.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/shmem.h"

PG_MODULE_MAGIC;

void	_PG_init(void);

static bool	got_sigterm = false;

static void
bgworker_example_sigterm(SIGNAL_ARGS)
{
	int			save_errno = errno;

	got_sigterm = true;
	if (MyProc)
		SetLatch(&MyProc->procLatch);

	errno = save_errno;
}

static void
bgworker_example_sighup(SIGNAL_ARGS)
{
	elog(LOG, "got sighup!");
	if (MyProc)
		SetLatch(&MyProc->procLatch);
}

static void
bgworker_example_main(void *main_arg)
{
	BackgroundWorkerUnblockSignals();
	
	while (!got_sigterm)
	{

		int		rc;

		/*
		 * Background workers mustn't call usleep() or any direct equivalent:
		 * instead, they may wait on their process latch, which sleeps as
		 * necessary, but is awakened if postmaster dies.  That way the
		 * background process goes away immediately in an emergency.
		 */
		elog(LOG, "In bgworker process %s %p", MyBgworkerEntry->bgw_name, MyProc);
		rc = WaitLatch(&MyProc->procLatch,
					   WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
					   1000L);
		ResetLatch(&MyProc->procLatch);

		/* emergency bailout if postmaster has died */
		if (rc & WL_POSTMASTER_DEATH)
			proc_exit(1);
	}

	proc_exit(1);
}

/*
 * Entrypoint of this module.
 *
 * We register three worker processes here, to demonstrate how that can be done.
 */
void
_PG_init(void)
{
	BackgroundWorker	worker;

	elog(LOG, "_PG_init from bgworker_example");

	/* register the worker processes.  These values are common for both */
	worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
	worker.bgw_main = bgworker_example_main;
	worker.bgw_sighup = bgworker_example_sighup;
	worker.bgw_sigterm = bgworker_example_sigterm;
	worker.bgw_restart_time = 5;

	worker.bgw_name = "Example worker 1";
	RegisterBackgroundWorker(&worker);

	worker.bgw_name = "Example worker 2";
	RegisterBackgroundWorker(&worker);

	worker.bgw_name = "Example worker 3";
	RegisterBackgroundWorker(&worker);
}
