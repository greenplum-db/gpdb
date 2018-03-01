/*-------------------------------------------------------------------------
 *
 * fts.c
 *	  Process under QD postmaster polls the segments on a periodic basis
 *    or at the behest of QEs.
 *
 * Maintains an array in shared memory containing the state of each segment.
 *
 * Portions Copyright (c) 2005-2010, Greenplum Inc.
 * Portions Copyright (c) 2011, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/fts/fts.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>

#include "access/genam.h"
#include "access/heapam.h"
#include "access/xact.h"
#include "catalog/gp_segment_config.h"
#include "catalog/indexing.h"
#include "catalog/pg_authid.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "cdb/cdbvars.h"
#include "libpq-fe.h"
#include "libpq-int.h"
#include "cdb/cdbfts.h"
#include "postmaster/fork_process.h"
#include "postmaster/postmaster.h"
#include "postmaster/fts.h"
#include "postmaster/ftsprobe.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "storage/procsignal.h"
#include "storage/pmsignal.h"			/* PostmasterIsAlive */
#include "storage/sinvaladt.h"
#include "utils/builtins.h"
#include "utils/faultinjector.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"
#include "utils/relcache.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/tqual.h"

#include "catalog/pg_authid.h"
#include "catalog/pg_database.h"
#include "catalog/pg_tablespace.h"
#include "catalog/catalog.h"

#include "catalog/gp_configuration_history.h"
#include "catalog/gp_segment_config.h"

#include "storage/backendid.h"

#include "executor/spi.h"

#include "tcop/tcopprot.h" /* quickdie() */


/*
 * CONSTANTS
 */
/* maximum number of segments */
#define MAX_NUM_OF_SEGMENTS  32768

bool am_ftshandler = false;

#define GpConfigHistoryRelName    "gp_configuration_history"


/*
 * STATIC VARIABLES
 */

static bool am_ftsprobe = false;
static bool skipFtsProbe = false;

static volatile bool shutdown_requested = false;
static volatile bool fullscan_requested = false;
static volatile sig_atomic_t got_SIGHUP = false;

static char *probeDatabase = "postgres";

/*
 * FUNCTION PROTOTYPES
 */

#ifdef EXEC_BACKEND
static pid_t ftsprobe_forkexec(void);
#endif
NON_EXEC_STATIC void ftsMain(int argc, char *argv[]);
static void FtsLoop(void);

static CdbComponentDatabases *readCdbComponentInfoAndUpdateStatus(MemoryContext);

/*
 * Main entry point for ftsprobe process.
 *
 * This code is heavily based on pgarch.c, q.v.
 */
int
ftsprobe_start(void)
{
	pid_t		FtsProbePID;

#ifdef EXEC_BACKEND
	switch ((FtsProbePID = ftsprobe_forkexec()))
#else
	switch ((FtsProbePID = fork_process()))
#endif
	{
		case -1:
			ereport(LOG,
					(errmsg("could not fork ftsprobe process: %m")));
			return 0;

#ifndef EXEC_BACKEND
		case 0:
			/* in postmaster child ... */
			/* Close the postmaster's sockets */
			ClosePostmasterPorts(false);

			ftsMain(0, NULL);
			break;
#endif
		default:
			return (int)FtsProbePID;
	}

	
	/* shouldn't get here */
	return 0;
}


/*=========================================================================
 * HELPER FUNCTIONS
 */


#ifdef EXEC_BACKEND
/*
 * ftsprobe_forkexec()
 *
 * Format up the arglist for the ftsprobe process, then fork and exec.
 */
static pid_t
ftsprobe_forkexec(void)
{
	char	   *av[10];
	int			ac = 0;

	av[ac++] = "postgres";
	av[ac++] = "--forkftsprobe";
	av[ac++] = NULL;			/* filled in by postmaster_forkexec */
	av[ac] = NULL;

	Assert(ac < lengthof(av));

	return postmaster_forkexec(ac, av);
}
#endif   /* EXEC_BACKEND */

inline bool
IsFtsShudownRequested(void) {
	return shutdown_requested;
}

static void
RequestShutdown(SIGNAL_ARGS)
{
	shutdown_requested = true;
}

/* SIGHUP: set flag to reload config file */
static void
sigHupHandler(SIGNAL_ARGS)
{
	got_SIGHUP = true;
}

/* SIGINT: set flag to indicate a FTS scan is requested */
static void
sigIntHandler(SIGNAL_ARGS)
{
	fullscan_requested = true;
}

/*
 * FtsProbeMain
 */
NON_EXEC_STATIC void
ftsMain(int argc, char *argv[])
{
	sigjmp_buf	local_sigjmp_buf;
	char	   *fullpath;

	IsUnderPostmaster = true;
	am_ftsprobe = true;

	/* Stay away from PMChildSlot */
	MyPMChildSlot = -1;

	/* reset MyProcPid */
	MyProcPid = getpid();
	
	/* Lose the postmaster's on-exit routines */
	on_exit_reset();

	/* Identify myself via ps */
	init_ps_display("ftsprobe process", "", "", "");

	SetProcessingMode(InitProcessing);

	/*
	 * reread postgresql.conf if requested
	 */
	pqsignal(SIGHUP, sigHupHandler);
	pqsignal(SIGINT, sigIntHandler);
	pqsignal(SIGTERM, die);
	pqsignal(SIGQUIT, quickdie); /* we don't do any ftsprobe specific cleanup, just use the standard. */
	pqsignal(SIGALRM, handle_sig_alarm);

	pqsignal(SIGPIPE, SIG_IGN);
	pqsignal(SIGUSR1, procsignal_sigusr1_handler);
	/* We don't listen for async notifies */
	pqsignal(SIGUSR2, RequestShutdown);
	pqsignal(SIGFPE, FloatExceptionHandler);
	pqsignal(SIGCHLD, SIG_DFL);

	/*
	 * Copied from bgwriter
	 */
	CurrentResourceOwner = ResourceOwnerCreate(NULL, "FTS Probe");

	/* Early initialization */
	BaseInit();

	/* See InitPostgres()... */
	InitProcess();	
	InitBufferPoolBackend();
	InitXLOGAccess();

	SetProcessingMode(NormalProcessing);

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

		AbortCurrentTransaction();

		/*
		 * We can now go away.	Note that because we'll call InitProcess, a
		 * callback will be registered to do ProcKill, which will clean up
		 * necessary state.
		 */
		proc_exit(0);
	}

	/* We can now handle ereport(ERROR) */
	PG_exception_stack = &local_sigjmp_buf;

	PG_SETMASK(&UnBlockSig);

	/*
	 * Add my PGPROC struct to the ProcArray.
	 *
	 * Once I have done this, I am visible to other backends!
	 */
	InitProcessPhase2();

	/*
	 * Initialize my entry in the shared-invalidation manager's array of
	 * per-backend data.
	 *
	 * Sets up MyBackendId, a unique backend identifier.
	 */
	MyBackendId = InvalidBackendId;

	SharedInvalBackendInit(false);

	if (MyBackendId > MaxBackends || MyBackendId <= 0)
		elog(FATAL, "bad backend id: %d", MyBackendId);

	/*
	 * bufmgr needs another initialization call too
	 */
	InitBufferPoolBackend();

	/* heap access requires the rel-cache */
	RelationCacheInitialize();
	InitCatalogCache();

	/*
	 * It's now possible to do real access to the system catalogs.
	 *
	 * Load relcache entries for the system catalogs.  This must create at
	 * least the minimum set of "nailed-in" cache entries.
	 */
	RelationCacheInitializePhase2();

	/*
	 * Start a new transaction here before first access to db, and get a
	 * snapshot.  We don't have a use for the snapshot itself, but we're
	 * interested in the secondary effect that it sets RecentGlobalXmin.
	 */
	StartTransactionCommand();
	(void) GetTransactionSnapshot();

	/*
	 * In order to access the catalog, we need a database, and a
	 * tablespace; our access to the heap is going to be slightly
	 * limited, so we'll just use some defaults.
	 */
	if (!FindMyDatabase(probeDatabase, &MyDatabaseId, &MyDatabaseTableSpace))
		ereport(FATAL,
				(errcode(ERRCODE_UNDEFINED_DATABASE),
				 errmsg("database \"%s\" does not exit", probeDatabase)));

	/* Now we can mark our PGPROC entry with the database ID */
	/* (We assume this is an atomic store so no lock is needed) */
	MyProc->databaseId = MyDatabaseId;

	fullpath = GetDatabasePath(MyDatabaseId, MyDatabaseTableSpace);

	SetDatabasePath(fullpath);

	RelationCacheInitializePhase3();

	/* close the transaction we started above */
	CommitTransactionCommand();

	/* shmem: publish probe pid */
	ftsProbeInfo->fts_probePid = MyProcPid;

	/* main loop */
	FtsLoop();

	/* One iteration done, go away */
	proc_exit(0);
}

/*
 * Populate cdb_component_dbs object by reading from catalog.  Use
 * probeContext instead of current memory context because current
 * context will be destroyed by CommitTransactionCommand().
 */
static
CdbComponentDatabases *readCdbComponentInfoAndUpdateStatus(MemoryContext probeContext)
{
	int i;
	MemoryContext save = MemoryContextSwitchTo(probeContext);
	/* cdbs is free'd by FtsLoop(). */
	CdbComponentDatabases *cdbs = getCdbComponentInfo(false);
	MemoryContextSwitchTo(save);

	for (i=0; i < cdbs->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *segInfo = &cdbs->segment_db_info[i];
		uint8	segStatus = 0;

		if (SEGMENT_IS_ALIVE(segInfo))
			FTS_STATUS_SET_UP(segStatus);

		ftsProbeInfo->fts_status[segInfo->dbid] = segStatus;
	}

	ftsProbeInfo->fts_status_initialized = true;
	return cdbs;
}

static void
probeWalRepUpdateConfig(int16 dbid, int16 segindex, char role,
						bool IsSegmentAlive, bool IsInSync)
{
	Assert(IsInSync ? IsSegmentAlive : true);

	/*
	 * Insert new tuple into gp_configuration_history catalog.
	 */
	{
		Relation histrel;
		HeapTuple histtuple;
		Datum histvals[Natts_gp_configuration_history];
		bool histnulls[Natts_gp_configuration_history] = { false };
		char desc[SQL_CMD_BUF_SIZE];

		histrel = heap_open(GpConfigHistoryRelationId,
							RowExclusiveLock);

		histvals[Anum_gp_configuration_history_time-1] =
				TimestampTzGetDatum(GetCurrentTimestamp());
		histvals[Anum_gp_configuration_history_dbid-1] =
				Int16GetDatum(dbid);
		snprintf(desc, sizeof(desc),
				 "FTS: update role, status, and mode for dbid %d with contentid %d to %c, %c, and %c",
				 dbid, segindex, role,
				 IsSegmentAlive ? GP_SEGMENT_CONFIGURATION_STATUS_UP :
				 GP_SEGMENT_CONFIGURATION_STATUS_DOWN,
				 IsInSync ? GP_SEGMENT_CONFIGURATION_MODE_INSYNC :
				 GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC
			);
		histvals[Anum_gp_configuration_history_desc-1] =
				CStringGetTextDatum(desc);
		histtuple = heap_form_tuple(RelationGetDescr(histrel), histvals, histnulls);
		simple_heap_insert(histrel, histtuple);
		CatalogUpdateIndexes(histrel, histtuple);

		SIMPLE_FAULT_INJECTOR(FtsUpdateConfig);

		heap_close(histrel, RowExclusiveLock);
	}

	/*
	 * Find and update gp_segment_configuration tuple.
	 */
	{
		Relation configrel;

		HeapTuple configtuple;
		HeapTuple newtuple;

		Datum configvals[Natts_gp_segment_configuration];
		bool confignulls[Natts_gp_segment_configuration] = { false };
		bool repls[Natts_gp_segment_configuration] = { false };

		ScanKeyData scankey;
		SysScanDesc sscan;

		configrel = heap_open(GpSegmentConfigRelationId,
							  RowExclusiveLock);

		ScanKeyInit(&scankey,
					Anum_gp_segment_configuration_dbid,
					BTEqualStrategyNumber, F_INT2EQ,
					Int16GetDatum(dbid));
		sscan = systable_beginscan(configrel, GpSegmentConfigDbidIndexId,
								   true, SnapshotNow, 1, &scankey);

		configtuple = systable_getnext(sscan);

		if (!HeapTupleIsValid(configtuple))
		{
			elog(ERROR, "FTS cannot find dbid=%d in %s", dbid,
				 RelationGetRelationName(configrel));
		}

		configvals[Anum_gp_segment_configuration_role-1] = CharGetDatum(role);
		repls[Anum_gp_segment_configuration_role-1] = true;

		configvals[Anum_gp_segment_configuration_status-1] =
			CharGetDatum(IsSegmentAlive ? GP_SEGMENT_CONFIGURATION_STATUS_UP :
										GP_SEGMENT_CONFIGURATION_STATUS_DOWN);
		repls[Anum_gp_segment_configuration_status-1] = true;

		configvals[Anum_gp_segment_configuration_mode-1] =
			CharGetDatum(IsInSync ? GP_SEGMENT_CONFIGURATION_MODE_INSYNC :
						 GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);
		repls[Anum_gp_segment_configuration_mode-1] = true;

		newtuple = heap_modify_tuple(configtuple, RelationGetDescr(configrel),
									 configvals, confignulls, repls);
		simple_heap_update(configrel, &configtuple->t_self, newtuple);
		CatalogUpdateIndexes(configrel, newtuple);

		systable_endscan(sscan);
		pfree(newtuple);

		heap_close(configrel, RowExclusiveLock);
	}
}

/*
 * Timeout is said to have occurred if greater than gp_fts_probe_timeout
 * seconds have elapsed since connection start and a response is not received.
 * Segments for which a response is received already or that have failed with
 * all retries exhausted are exempted from timeout evaluation.
 */
static void
ftsCheckTimeout(per_segment_info *ftsInfo, pg_time_t now)
{
	if (!IsFtsProbeStateSuccess(ftsInfo->state) &&
		ftsInfo->retry_count < gp_fts_probe_retries &&
		(int) (now - ftsInfo->startTime) > gp_fts_probe_timeout)
	{
		elog(LOG,
			 "FTS timeout detected for (content=%d, dbid=%d) "
			 "state=%d, retry_count=%d,",
			 ftsInfo->primary_cdbinfo->segindex,
			 ftsInfo->primary_cdbinfo->dbid, ftsInfo->state,
			 ftsInfo->retry_count);
		ftsInfo->state = nextFailedState(ftsInfo->state);
		/* For now no more retries if connection timed out. */

		ftsInfo->retry_count = gp_fts_probe_retries;
	}
}

/*
 * Return true for segments whose response is ready to be processed.  Segments
 * whose response is already processed should have response->conn set to NULL.
 */
static bool
ftsResponseReady(per_segment_info *ftsInfo)
{
	return ((IsFtsProbeStateSuccess(ftsInfo->state) ||
			 IsFtsProbeStateFailed(ftsInfo->state)) && ftsInfo->conn != NULL);
}

/*
 * Process resonses from primary segments:
 * (a) Transition internal state so that segments can be messaged subsequently
 * (e.g. promotion and turning off syncrep).
 * (b) Update gp_segment_configuration catalog table, if needed.
 */
bool
probeWalRepPublishUpdate(fts_context *context)
{
	bool is_updated = false;

	pg_time_t now = (pg_time_t) time(NULL);

	for (int response_index = 0; response_index < context->num_pairs;
		 response_index ++)
	{
		per_segment_info *ftsInfo = &(context->perSegInfos[response_index]);

		ftsCheckTimeout(ftsInfo, now);
		/*
		 * Consider segments that are in final state (success / failure) and
		 * that are not already processed.
		 */
		if (!ftsResponseReady(ftsInfo))
			continue;
		/*
		 * We must be in a final state, which is (1) success or (2) failure
		 * with retries exhausted.
		 */
		Assert(IsFtsProbeStateSuccess(ftsInfo->state) ||
			   IsFtsProbeStateFailed(ftsInfo->state));
		AssertImply(IsFtsProbeStateFailed(ftsInfo->state),
					ftsInfo->retry_count == gp_fts_probe_retries);

		/*
		 * Before sending promote message, primary_cdbinfo refence is swapped
		 * with the mirror, leaving primary_cdbinfo->role to be 'm'.  So the
		 * assertion applies only to non-promote cases.
		 */
		AssertImply(ftsInfo->state != FTS_PROMOTE_SUCCESS &&
					ftsInfo->state != FTS_PROMOTE_FAILED,
					SEGMENT_IS_ACTIVE_PRIMARY(ftsInfo->primary_cdbinfo));

		if (!FtsIsActive())
			return false;

		CdbComponentDatabaseInfo *primary = ftsInfo->primary_cdbinfo;

		CdbComponentDatabaseInfo *mirror = ftsInfo->mirror_cdbinfo;

		bool UpdatePrimary = false;
		bool UpdateMirror = false;

		/*
		 * Currently, mode of primary and mirror should be same, either both
		 * reflecting in sync or not in sync.
		 */
		Assert(primary->mode == mirror->mode);

		bool IsPrimaryAlive = ftsInfo->result.isPrimaryAlive;
		/* Trust a response from primary only if it's alive. */
		bool IsMirrorAlive =  IsPrimaryAlive ?
			ftsInfo->result.isMirrorAlive : SEGMENT_IS_ALIVE(mirror);
		bool IsInSync = IsPrimaryAlive ?
			ftsInfo->result.isInSync : false;

		/* If primary and mirror are in sync, then both have to be ALIVE. */
		AssertImply(IsInSync, IsPrimaryAlive && IsMirrorAlive);
		/* Primary must enable syncrep as long as it thinks mirror is alive. */
		AssertImply(IsMirrorAlive && IsPrimaryAlive,
					ftsInfo->result.isSyncRepEnabled);

		/* Only swapped in promotion; by default, keep the current roles. */
		char newPrimaryRole = GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;
		char newMirrorRole = GP_SEGMENT_CONFIGURATION_ROLE_MIRROR;

		/*
		 * Allow updates to configuration upon receiving response to PROBE
		 * message.  Any other message (SYNCREP_OFF / PROMOTE) should not
		 * result in configuration update.
		 *
		 * XXX For SYNCREP_OFF message, a primary may respond with mirror is
		 * back up and syncrep is turned on.  In that case, the mirror should
		 * be marked back up in coniguration.
		 */
		if (ftsInfo->state == FTS_PROBE_SUCCESS ||
			ftsInfo->state == FTS_PROBE_FAILED)
		{
			UpdatePrimary = (IsPrimaryAlive != SEGMENT_IS_ALIVE(primary));
			UpdateMirror = (IsMirrorAlive != SEGMENT_IS_ALIVE(mirror));
			/*
			 * If probe response state is different from current state in
			 * configuration, update both primary and mirror.
			 */
			if (IsInSync != SEGMENT_IS_IN_SYNC(primary))
				UpdatePrimary = UpdateMirror = true;
		}

		switch(ftsInfo->state)
		{
			case FTS_PROBE_SUCCESS:
				Assert(IsPrimaryAlive);
				if (ftsInfo->result.isSyncRepEnabled && !IsMirrorAlive)
				{
					/*
					 * Primaries that have syncrep enabled continue to block
					 * commits.  FTS must notify them to unblock commits by
					 * sending syncrep off message.
					 */
					ftsInfo->state = FTS_SYNCREP_SEGMENT;
					elog(LOG, "FTS turning syncrep off on (content=%d, dbid=%d)",
						 primary->segindex, primary->dbid);
					/*
					 * Mirror must be marked down in FTS configuration before
					 * primary can be notified to unblock commits.
					 */
					Assert(UpdateMirror || !SEGMENT_IS_ALIVE(mirror));
				}
				else if (ftsInfo->result.isRoleMirror && !IsMirrorAlive)
				{
					/*
					 * A promote message sent previously didn't make it to the
					 * mirror.  Catalog must have been updated before sending
					 * the previous promote message.
					 */
					Assert(!SEGMENT_IS_ALIVE(mirror));
					Assert(SEGMENT_IS_NOT_INSYNC(mirror));
					Assert(SEGMENT_IS_NOT_INSYNC(primary));
					Assert(!ftsInfo->result.isSyncRepEnabled);
					Assert(!UpdateMirror);
					Assert(!UpdatePrimary);
					elog(LOG, "FTS resending promote request to (content=%d,"
						 " dbid=%d)", primary->segindex, primary->dbid);
					ftsInfo->state = FTS_PROMOTE_SEGMENT;
				}
				break;
			case FTS_PROBE_FAILED:
				/* Primary is down, see if mirror can be promoted. */
				Assert(!IsPrimaryAlive);
				if (SEGMENT_IS_IN_SYNC(mirror))
				{
					/*
					 * Primary must have been recorded as in-sync before the
					 * probe.
					 */
					Assert(SEGMENT_IS_IN_SYNC(primary));
					/*
					 * Swap the primary and mirror references so that the
					 * mirror will be promoted in subsequent connect, poll,
					 * send, receive steps.
					 */
					elog(LOG, "FTS promoting mirror (content=%d, dbid=%d) to be"
						 " the new primary", mirror->segindex, mirror->dbid);
					ftsInfo->state = FTS_PROMOTE_SEGMENT;
					ftsInfo->primary_cdbinfo = mirror;
					ftsInfo->mirror_cdbinfo = primary;

					/*
					 * Flip the roles and mark the failed primary as down in
					 * FTS configuration before sending promote message.
					 * Dispatcher should no longer consider the failed primary
					 * for gang creation, FTS should no longer probe the failed
					 * primary.
					 */
					newPrimaryRole = GP_SEGMENT_CONFIGURATION_ROLE_MIRROR;
					newMirrorRole = GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;
				}
				else
				{
					elog(WARNING, "FTS double fault detected (content=%d) "
						 "primary dbid=%d, mirror dbid=%d",
						 primary->segindex, primary->dbid, mirror->dbid);
					UpdatePrimary = UpdateMirror = false;
				}
				break;
			case FTS_SYNCREP_FAILED:
				/*
				 * XXX transactions may block forever, waiting for a mirror
				 * that doesn't exist.  Should we PANIC here?
				 */
				elog(WARNING, "FTS failed to turn off syncrep on (content=%d,"
					 " dbid=%d)", primary->segindex, primary->dbid);
				break;
			case FTS_PROMOTE_FAILED:
				elog(WARNING, "FTS double fault detected (content=%d) "
					 "primary dbid=%d, mirror dbid=%d",
					 primary->segindex, primary->dbid, mirror->dbid);
				break;
			case FTS_PROMOTE_SUCCESS:
				Assert(ftsInfo->result.isRoleMirror);
				elog(LOG, "FTS mirror (content=%d, dbid=%d) promotion "
					 "triggerred successfully",
					 primary->segindex, primary->dbid);
				break;
			case FTS_SYNCREP_SUCCESS:
				elog(LOG, "FTS primary (content=%d, dbid=%d) notified to turn "
					 "syncrep off", primary->segindex, primary->dbid);
				break;
			default:
				elog(ERROR, "FTS invalid internal state %d for (content=%d)"
					 "primary dbid=%d, mirror dbid=%d", ftsInfo->state,
					 primary->segindex, primary->dbid, mirror->dbid);
				break;
		}
		/* Close connection and reset result for next message, if any. */
		memset(&ftsInfo->result, 0, sizeof(probe_result));
		PQfinish(ftsInfo->conn);
		ftsInfo->conn = NULL;
		ftsInfo->poll_events = ftsInfo->poll_revents = 0;
		ftsInfo->retry_count = 0;

		if (shutdown_requested)
		{
			elog(LOG, "shutdown in progress, ignoring FTS prober updates");
			return is_updated;
		}

		if (UpdatePrimary || UpdateMirror)
		{
			/*
			 * Commit/abort transaction below will destroy
			 * CurrentResourceOwner.  We need it for catalog reads.
			 */
			ResourceOwner save = CurrentResourceOwner;
			StartTransactionCommand();
			GetTransactionSnapshot();

			if (UpdatePrimary)
				probeWalRepUpdateConfig(primary->dbid, primary->segindex,
										newPrimaryRole, IsPrimaryAlive,
										IsInSync);

			if (UpdateMirror)
				probeWalRepUpdateConfig(mirror->dbid, mirror->segindex,
										newMirrorRole, IsMirrorAlive,
										IsInSync);

			is_updated = true;

			CommitTransactionCommand();
			CurrentResourceOwner = save;

			/*
			 * Update the status to in-memory variable as well used by
			 * dispatcher, now that changes has been persisted to catalog.
			 */
			Assert(ftsProbeInfo);
			/* XXX why shouldn't FtsControlBlock->ControlLock be held here? */
			if (IsPrimaryAlive)
				FTS_STATUS_SET_UP(ftsProbeInfo->fts_status[primary->dbid]);
			else
				FTS_STATUS_SET_DOWN(ftsProbeInfo->fts_status[primary->dbid]);

			if (IsMirrorAlive)
				FTS_STATUS_SET_UP(ftsProbeInfo->fts_status[mirror->dbid]);
			else
				FTS_STATUS_SET_DOWN(ftsProbeInfo->fts_status[mirror->dbid]);
		}
	}

	return is_updated;
}

static void
FtsWalRepInitProbeContext(CdbComponentDatabases *cdbs, fts_context *context)
{
	context->num_pairs = cdbs->total_segments;
	context->perSegInfos = (per_segment_info *) palloc0(
		context->num_pairs * sizeof(per_segment_info));

	int response_index = 0;

	for(int segment_index = 0; segment_index < cdbs->total_segment_dbs; segment_index++)
	{
		CdbComponentDatabaseInfo *segment = &(cdbs->segment_db_info[segment_index]);
		per_segment_info *ftsInfo = &(context->perSegInfos[response_index]);

		if (!SEGMENT_IS_ACTIVE_PRIMARY(segment))
			continue;

		/*
		 * We initialize the probe_result.IsPrimaryAlive and
		 * probe_result.IsMirrorAlive to current state, and that will prevent
		 * changing of mirror status if the primary goes down.
		 */
		CdbComponentDatabaseInfo *primary = segment;
		CdbComponentDatabaseInfo *mirror = FtsGetPeerSegment(cdbs,
															 primary->segindex,
															 primary->dbid);

		/*
		 * If there is no mirror under this primary, no need to probe.
		 */
		if (!mirror)
		{
			context->num_pairs--;
			continue;
		}

		/* primary in catalog will NEVER be marked down. */
		Assert(FtsIsSegmentAlive(primary));

		/*
		 * Before we probe, we always DEFAULT the response for primary is
		 * down. This will only be changed to true if primary is alive,
		 * otherwise some error happened during the probe. Similarly, always
		 * DEFAULT the response to not in sync. Only if primary communicates
		 * positively, consider it in sync.
		 */
		ftsInfo->result.isPrimaryAlive = false;
		ftsInfo->result.isMirrorAlive = false;
		ftsInfo->result.isInSync = false;
		ftsInfo->result.isSyncRepEnabled = false;
		ftsInfo->result.retryRequested = false;
		ftsInfo->result.isRoleMirror = false;
		ftsInfo->result.dbid = primary->dbid;
		ftsInfo->state = FTS_PROBE_SEGMENT;

		ftsInfo->primary_cdbinfo = primary;
		ftsInfo->mirror_cdbinfo = mirror;

		Assert(response_index < context->num_pairs);
		response_index ++;
	}
}

static
void FtsLoop()
{
	bool	updated_probe_state;
	MemoryContext probeContext = NULL, oldContext = NULL;
	time_t elapsed,	probe_start_time;
	CdbComponentDatabases *cdbs = NULL;

	probeContext = AllocSetContextCreate(TopMemoryContext,
										 "FtsProbeMemCtxt",
										 ALLOCSET_DEFAULT_INITSIZE,	/* always have some memory */
										 ALLOCSET_DEFAULT_INITSIZE,
										 ALLOCSET_DEFAULT_MAXSIZE);

	for (;;)
	{
		bool		has_mirrors;

		if (shutdown_requested)
			break;
		/* no need to live on if postmaster has died */
		if (!PostmasterIsAlive(true))
			exit(1);

		if (got_SIGHUP)
		{
			got_SIGHUP = false;
			ProcessConfigFile(PGC_SIGHUP);
		}

		probe_start_time = time(NULL);

		skipFtsProbe = false;

#ifdef FAULT_INJECTOR
		if (SIMPLE_FAULT_INJECTOR(FtsProbe) == FaultInjectorTypeSkip)
			skipFtsProbe = true;
#endif

		/* make sure the probeScanRequested is set before SIGINT sent to FTS */
		AssertImply(fullscan_requested,
					ftsProbeInfo->fts_probeScanRequested ==
					ftsProbeInfo->fts_statusVersion);

		if (skipFtsProbe)
		{
			if (gp_log_fts >= GPVARS_VERBOSITY_VERBOSE)
				elog(LOG, "skipping FTS probes");
			if (fullscan_requested)
			{
				ftsProbeInfo->fts_statusVersion++;
				fullscan_requested = false;
				if (gp_log_fts >= GPVARS_VERBOSITY_VERBOSE)
					elog(LOG, "skipping fullscan, since probe is skipped.");
			}
			goto prober_sleep;
		}

		if (cdbs != NULL)
		{
			freeCdbComponentDatabases(cdbs);
			cdbs = NULL;
		}

		/* Need a transaction to access the catalogs */
		StartTransactionCommand();

		cdbs = readCdbComponentInfoAndUpdateStatus(probeContext);

		/* Check here gp_segment_configuration if has mirror's */
		has_mirrors = gp_segment_config_has_mirrors();

		/* close the transaction we started above */
		CommitTransactionCommand();

		if (!has_mirrors)
		{
			/* The dispatcher could have requested a scan so just ignore it and unblock the dispatcher */
			if (fullscan_requested)
			{
				ftsProbeInfo->fts_statusVersion++;
				fullscan_requested = false;
			}
			goto prober_sleep;
		}

		elog(DEBUG3, "FTS: starting %s scan with %d segments and %d contents",
			 (fullscan_requested ? "full " : ""),
			 cdbs->total_segment_dbs,
			 cdbs->total_segments);

		/*
		 * We probe in a special context, some of the heap access
		 * stuff palloc()s internally
		 */
		oldContext = MemoryContextSwitchTo(probeContext);
		initPollFds(cdbs->total_segments);

		fts_context context;

		FtsWalRepInitProbeContext(cdbs, &context);

		updated_probe_state = FtsWalRepMessageSegments(&context);

		MemoryContextSwitchTo(oldContext);

		/* free any pallocs we made inside probeSegments() */
		MemoryContextReset(probeContext);
		cdbs = NULL;

		if (!FtsIsActive())
		{
			if (gp_log_fts >= GPVARS_VERBOSITY_VERBOSE)
				elog(LOG, "FTS: skipping probe, FTS is paused or shutting down.");
			goto prober_sleep;
		}

		/*
		 * bump the version (this also serves as an acknowledgement to
		 * a probe-request).
		 */
		if (updated_probe_state || fullscan_requested)
		{
			ftsProbeInfo->fts_statusVersion++;
			fullscan_requested = false;
		}

	prober_sleep:
		{

			/* check if we need to sleep before starting next iteration */
			elapsed = time(NULL) - probe_start_time;
			if (elapsed < gp_fts_probe_interval && !shutdown_requested)
			{
				pg_usleep((gp_fts_probe_interval - elapsed) * USECS_PER_SEC);

				CHECK_FOR_INTERRUPTS();
			}
		}
	} /* end server loop */

	return;
}

/*
 * Check if FTS is active
 */
bool
FtsIsActive(void)
{
	return (!skipFtsProbe && !shutdown_requested);
}

bool
FtsIsSegmentAlive(CdbComponentDatabaseInfo *segInfo)
{
	if (SEGMENT_IS_ACTIVE_MIRROR(segInfo) && SEGMENT_IS_ALIVE(segInfo))
		return true;

	if (SEGMENT_IS_ACTIVE_PRIMARY(segInfo))
		return true;

	return false;
}

/*
 * Get peer segment descriptor
 */
CdbComponentDatabaseInfo *FtsGetPeerSegment(CdbComponentDatabases *cdbs,
											int content, int dbid)
{
	int i;

	for (i=0; i < cdbs->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *segInfo = &cdbs->segment_db_info[i];

		if (segInfo->segindex == content && segInfo->dbid != dbid)
		{
			/* found it */
			return segInfo;
		}
	}

	return NULL;
}
