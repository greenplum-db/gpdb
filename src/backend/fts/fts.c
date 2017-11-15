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
#include "catalog/gp_segment_config.h"
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
#ifndef USE_SEGWALREP
/* one byte of status for each segment */
static uint8 scan_status[MAX_NUM_OF_SEGMENTS];
#endif

#define GpConfigHistoryRelName    "gp_configuration_history"


/*
 * STATIC VARIABLES
 */

static bool am_ftsprobe = false;

static volatile bool shutdown_requested = false;
static volatile bool rescan_requested = false;
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

/* SIGINT: set flag to run an fts full-scan */
static void
ReqFtsFullScan(SIGNAL_ARGS)
{
	rescan_requested = true;
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

	/*
	 * Presently, SIGINT will lead to autovacuum shutdown, because that's how
	 * we handle ereport(ERROR).  It could be improved however.
	 */
	pqsignal(SIGINT, ReqFtsFullScan);		/* request full-scan */
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
		uint8	segStatus;

		segStatus = 0;

		if (SEGMENT_IS_ALIVE(segInfo))
			segStatus |= FTS_STATUS_ALIVE;

		if (SEGMENT_IS_ACTIVE_PRIMARY(segInfo))
			segStatus |= FTS_STATUS_PRIMARY;

		if (segInfo->preferred_role == GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY)
			segStatus |= FTS_STATUS_DEFINEDPRIMARY;

		if (segInfo->mode == GP_SEGMENT_CONFIGURATION_MODE_INSYNC)
			segStatus |= FTS_STATUS_SYNCHRONIZED;

		if (segInfo->mode == GP_SEGMENT_CONFIGURATION_MODE_CHANGETRACKING)
			segStatus |= FTS_STATUS_CHANGELOGGING;

		ftsProbeInfo->fts_status[segInfo->dbid] = segStatus;
	}

	return cdbs;
}

#ifdef USE_SEGWALREP
static void
probeWalRepUpdateConfig(int16 dbid, int16 segindex, bool IsSegmentAlive, bool IsInSync)
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
				 "FTS: update status and mode for dbid %d with contentid %d to %c and %c",
				 dbid, segindex,
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

static bool
probeWalRepPublishUpdate(CdbComponentDatabases *cdbs, probe_context *context)
{
	bool is_updated = false;

	for (int response_index = 0; response_index < context->count; response_index ++)
	{
		probe_response_per_segment *response = &(context->responses[response_index]);

		Assert(SEGMENT_IS_ACTIVE_PRIMARY(response->segment_db_info));

		if (!FtsIsActive())
			return false;

		CdbComponentDatabaseInfo *primary = response->segment_db_info;

		CdbComponentDatabaseInfo *mirror = FtsGetPeerSegment(cdbs,
															 primary->segindex,
															 primary->dbid);

		/*
		 * Currently, mode of primary and mirror should be same, either both
		 * reflecting in sync or not in sync.
		 */
		Assert(primary->mode == mirror->mode);

		bool IsPrimaryAlive = response->result.isPrimaryAlive;
		bool IsMirrorAlive = response->result.isMirrorAlive;
		bool IsInSync = response->result.isInSync;

		/* If are in sync, then both have to be ALIVE */
		Assert(IsInSync ? (IsPrimaryAlive && IsMirrorAlive) : true);

		bool UpdatePrimary = (IsPrimaryAlive != SEGMENT_IS_ALIVE(primary));
		bool UpdateMirror = (IsMirrorAlive != SEGMENT_IS_ALIVE(mirror));

		if (IsInSync != SEGMENT_IS_IN_SYNC(primary))
			UpdatePrimary = UpdateMirror = true;

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
										IsPrimaryAlive, IsInSync);

			if (UpdateMirror)
				probeWalRepUpdateConfig(mirror->dbid, mirror->segindex,
										IsMirrorAlive, IsInSync);

			if (shutdown_requested)
			{
				elog(LOG, "Shutdown in progress, ignoring FTS prober updates.");
				return is_updated;
			}

			is_updated = true;

			CommitTransactionCommand();
			CurrentResourceOwner = save;
		}

	}

	return is_updated;
}

static void
FtsWalRepInitProbeContext(CdbComponentDatabases *cdbs, probe_context *context)
{
	context->count = cdbs->total_segments;
	context->responses = (probe_response_per_segment *)palloc(context->count * sizeof(probe_response_per_segment));

	int response_index = 0;

	for(int segment_index = 0; segment_index < cdbs->total_segment_dbs; segment_index++)
	{
		CdbComponentDatabaseInfo *segment = &(cdbs->segment_db_info[segment_index]);
		probe_response_per_segment *response = &(context->responses[response_index]);

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

		/* primary in catalog will NEVER be marked down. */
		Assert(FtsIsSegmentAlive(primary));

		/*
		 * Before we probe, we always DEFAULT the response for primary is
		 * down. This will only be changed to true if primary is alive,
		 * otherwise some error happened during the probe. Similarly, always
		 * DEFAULT the response to not in sync. Only if primary communicates
		 * positively, consider it in sync.
		 */
		response->result.isPrimaryAlive = false;
		response->result.isMirrorAlive = SEGMENT_IS_ALIVE(mirror);
		response->result.isInSync = false;

		response->segment_db_info = primary;
		response->isScheduled = false;

		Assert(response_index < context->count);
		response_index ++;
	}
}
#endif

static
void FtsLoop()
{
	bool	updated_probe_state, processing_fullscan;
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

		ftsLock();

		/* atomically clear cancel flag and check pause flag */
		bool pauseProbes = ftsProbeInfo->fts_pauseProbes;
		ftsProbeInfo->fts_discardResults = false;

		ftsUnlock();

		if (pauseProbes)
		{
			if (gp_log_fts >= GPVARS_VERBOSITY_VERBOSE)
				elog(LOG, "skipping probe, we're paused.");
			goto prober_sleep;
		}

		if (cdbs != NULL)
		{
			freeCdbComponentDatabases(cdbs);
			cdbs = NULL;
		}

		if (ftsProbeInfo->fts_probeScanRequested == ftsProbeInfo->fts_statusVersion)
			processing_fullscan = true;
		else
			processing_fullscan = false;

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
			if (processing_fullscan)
			{
				ftsProbeInfo->fts_statusVersion = ftsProbeInfo->fts_statusVersion + 1;
				rescan_requested = false;
			}
			goto prober_sleep;
		}

		elog(DEBUG3, "FTS: starting %s scan with %d segments and %d contents",
			 (processing_fullscan ? "full " : ""),
			 cdbs->total_segment_dbs,
			 cdbs->total_segments);

		/*
		 * We probe in a special context, some of the heap access
		 * stuff palloc()s internally
		 */
		oldContext = MemoryContextSwitchTo(probeContext);

#ifdef USE_SEGWALREP
		probe_context context;

		FtsWalRepInitProbeContext(cdbs, &context);
		FtsWalRepProbeSegments(&context);

		updated_probe_state = probeWalRepPublishUpdate(cdbs, &context);
#else
		/* probe segments */
		FtsProbeSegments(cdbs, scan_status);

		/*
		 * Now we've completed the scan, update shared-memory. if we
		 * change anything, we return true.
		 */
		updated_probe_state = probePublishUpdate(cdbs, scan_status);
#endif

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
		 * If we're not processing a full-scan, but one has been requested; we start over.
		 */
		if (!processing_fullscan &&
			ftsProbeInfo->fts_probeScanRequested == ftsProbeInfo->fts_statusVersion)
			continue;

		/*
		 * bump the version (this also serves as an acknowledgement to
		 * a probe-request).
		 */
		if (updated_probe_state || processing_fullscan)
		{
			ftsProbeInfo->fts_statusVersion = ftsProbeInfo->fts_statusVersion + 1;
			rescan_requested = false;
		}

		/* if no full-scan has been requested, we can sleep. */
		if (ftsProbeInfo->fts_probeScanRequested >= ftsProbeInfo->fts_statusVersion)
		{
			/* we need to do a probe immediately */
			elog(LOG, "FTS: skipping sleep, requested version: %d, current version: %d.",
				 (int)ftsProbeInfo->fts_probeScanRequested, (int)ftsProbeInfo->fts_statusVersion);
			continue;
		}

	prober_sleep:
		{
			/* check if we need to sleep before starting next iteration */
			elapsed = time(NULL) - probe_start_time;
			if (elapsed < gp_fts_probe_interval && !shutdown_requested)
			{
				pg_usleep((gp_fts_probe_interval - elapsed) * USECS_PER_SEC);
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
	return (!ftsProbeInfo->fts_discardResults && !shutdown_requested);
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
 * Dump out the changes to our logfile.
 */
void
FtsDumpChanges(FtsSegmentStatusChange *changes, int changeEntries)
{
	Assert(changes != NULL);
	int i = 0;

	for (i = 0; i < changeEntries; i++)
	{
		bool new_alive, old_alive;
		bool new_pri, old_pri;

		new_alive = (changes[i].newStatus & FTS_STATUS_ALIVE ? true : false);
		old_alive = (changes[i].oldStatus & FTS_STATUS_ALIVE ? true : false);

		new_pri = (changes[i].newStatus & FTS_STATUS_PRIMARY ? true : false);
		old_pri = (changes[i].oldStatus & FTS_STATUS_PRIMARY ? true : false);

		elog(LOG, "FTS: change state for segment (dbid=%d, content=%d) from ('%c','%c') to ('%c','%c')",
			 changes[i].dbid,
			 changes[i].segindex,
			 (old_alive ? GP_SEGMENT_CONFIGURATION_STATUS_UP : GP_SEGMENT_CONFIGURATION_STATUS_DOWN),
			 (old_pri ? GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY : GP_SEGMENT_CONFIGURATION_ROLE_MIRROR),
			 (new_alive ? GP_SEGMENT_CONFIGURATION_STATUS_UP : GP_SEGMENT_CONFIGURATION_STATUS_DOWN),
			 (new_pri ? GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY : GP_SEGMENT_CONFIGURATION_ROLE_MIRROR));
	}
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


/*
 * Notify postmaster to shut down due to inconsistent segment state
 */
void FtsRequestPostmasterShutdown(CdbComponentDatabaseInfo *primary, CdbComponentDatabaseInfo *mirror)
{
	FtsRequestMasterShutdown();

	elog(FATAL, "FTS: detected invalid state for content=%d: "
			    "primary (dbid=%d, mode='%c', status='%c'), "
			    "mirror (dbid=%d, mode='%c', status='%c'), "
			    "shutting down master.",
			    primary->segindex,
			    primary->dbid,
			    primary->mode,
			    primary->status,
			    mirror->dbid,
			    mirror->mode,
			    mirror->status
			    );
}

/* EOF */
