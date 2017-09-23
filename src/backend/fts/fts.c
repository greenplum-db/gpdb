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
#include "gp-libpq-fe.h"
#include "gp-libpq-int.h"
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
#include "utils/syscache.h"

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

/* buffer size for SQL command */
#define SQL_CMD_BUF_SIZE     1024

#define GpConfigHistoryRelName    "gp_configuration_history"


/*
 * STATIC VARIABLES
 */

#ifndef USE_SEGWALREP
/* one byte of status for each segment */
static uint8 scan_status[MAX_NUM_OF_SEGMENTS];
#endif

static bool am_ftsprobe = false;

static volatile bool shutdown_requested = false;
static volatile bool rescan_requested = false;
static volatile sig_atomic_t got_SIGHUP = false;

static char *probeDatabase = "postgres";

/* struct holding segment configuration */
static CdbComponentDatabases *cdb_component_dbs = NULL;


/*
 * FUNCTION PROTOTYPES
 */

#ifdef EXEC_BACKEND
static pid_t ftsprobe_forkexec(void);
#endif
NON_EXEC_STATIC void ftsMain(int argc, char *argv[]);
static void FtsLoop(void);

static void readCdbComponentInfoAndUpdateStatus(MemoryContext probeContext);
static bool probePublishUpdate(uint8 *scan_status);

static uint32 getTransition(bool isPrimaryAlive, bool isMirrorAlive);

static void
buildSegmentStateChange
	(
	CdbComponentDatabaseInfo *segInfo,
	FtsSegmentStatusChange *change,
	uint8 statusNew
	)
	;

static uint32 transition
	(
	uint32 stateOld,
	uint32 trans,
	CdbComponentDatabaseInfo *primary,
	CdbComponentDatabaseInfo *mirror,
	FtsSegmentStatusChange *changesPrimary,
	FtsSegmentStatusChange *changesMirror
	)
	;

static void updateConfiguration(FtsSegmentStatusChange *changes, int changeEntries);
static bool probeUpdateConfig(FtsSegmentStatusChange *changes, int changeCount);

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
static void
readCdbComponentInfoAndUpdateStatus(MemoryContext probeContext)
{
	int i;
	MemoryContext save = MemoryContextSwitchTo(probeContext);
	/* cdb_component_dbs is free'd by FtsLoop(). */
	cdb_component_dbs = getCdbComponentInfo(false);
	MemoryContextSwitchTo(save);

	for (i=0; i < cdb_component_dbs->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *segInfo = &cdb_component_dbs->segment_db_info[i];
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
}

#ifdef USE_SEGWALREP
static void
probeWalRepUpdateConfig(int16 dbid, int16 segindex, bool IsSegmentAlive)
{
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
				"FTS: update status for dbid %d with contentid %d to %c",
				dbid, segindex,
				IsSegmentAlive ? GP_SEGMENT_CONFIGURATION_STATUS_UP :
					GP_SEGMENT_CONFIGURATION_STATUS_DOWN);
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
probeWalRepPublishUpdate(probe_context *context)
{
	bool is_updated = false;

	for (int response_index = 0; response_index < context->count; response_index ++)
	{
		probe_response_per_segment *response = &(context->responses[response_index]);

		Assert(SEGMENT_IS_ACTIVE_PRIMARY(response->segment_db_info));

		if (!FtsIsActive())
		{
			return false;
		}

		CdbComponentDatabaseInfo *primary = response->segment_db_info;
		CdbComponentDatabaseInfo *mirror = FtsGetPeerSegment(primary->segindex, primary->dbid);

		bool IsPrimaryAlive = response->result.isPrimaryAlive;
		bool IsMirrorAlive = response->result.isMirrorAlive;

		bool UpdatePrimary = (IsPrimaryAlive != SEGMENT_IS_ALIVE(primary));
		bool UpdateMirror = (IsMirrorAlive != SEGMENT_IS_ALIVE(mirror));

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
				probeWalRepUpdateConfig(primary->dbid, primary->segindex, IsPrimaryAlive);

			if (UpdateMirror)
				probeWalRepUpdateConfig(mirror->dbid, mirror->segindex, IsMirrorAlive);

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
	context->count = cdb_component_dbs->total_segments;
	context->responses = (probe_response_per_segment *)palloc(context->count * sizeof(probe_response_per_segment));

	int response_index = 0;

	for(int segment_index = 0; segment_index < cdbs->total_segment_dbs; segment_index++)
	{
		CdbComponentDatabaseInfo *segment = &(cdb_component_dbs->segment_db_info[segment_index]);
		probe_response_per_segment *response = &(context->responses[response_index]);

		if (!SEGMENT_IS_ACTIVE_PRIMARY(segment))
			continue;

		/*
		 * We initialize the probe_result.IsPrimaryAlive and
		 * probe_result.IsMirrorAlive to current state, and that will prevent
		 * changing of mirror status if the primary goes down.
		 */
		CdbComponentDatabaseInfo *primary = segment;
		CdbComponentDatabaseInfo *mirror = FtsGetPeerSegment(primary->segindex,
															 primary->dbid);

		response->result.isPrimaryAlive = SEGMENT_IS_ALIVE(primary);
		response->result.isMirrorAlive = SEGMENT_IS_ALIVE(mirror);

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

	probeContext = AllocSetContextCreate(TopMemoryContext,
										 "FtsProbeMemCtxt",
										 ALLOCSET_DEFAULT_INITSIZE,	/* always have some memory */
										 ALLOCSET_DEFAULT_INITSIZE,
										 ALLOCSET_DEFAULT_MAXSIZE);
	
	readCdbComponentInfoAndUpdateStatus(probeContext);

	for (;;)
	{
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

		if (cdb_component_dbs != NULL)
		{
			freeCdbComponentDatabases(cdb_component_dbs);
			cdb_component_dbs = NULL;
		}

		if (ftsProbeInfo->fts_probeScanRequested == ftsProbeInfo->fts_statusVersion)
			processing_fullscan = true;
		else
			processing_fullscan = false;

		readCdbComponentInfoAndUpdateStatus(probeContext);

		/* Check here gp_segment_configuration if has mirror's */
		if (! gp_segment_config_has_mirrors())
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
			 cdb_component_dbs->total_segment_dbs,
			 cdb_component_dbs->total_segments);

		/*
		 * We probe in a special context, some of the heap access
		 * stuff palloc()s internally
		 */
		oldContext = MemoryContextSwitchTo(probeContext);

#ifdef USE_SEGWALREP
		probe_context context;

		FtsWalRepInitProbeContext(cdb_component_dbs, &context);
		FtsWalRepProbeSegments(&context);

		updated_probe_state = probeWalRepPublishUpdate(&context);
#else
		/* probe segments */
		FtsProbeSegments(cdb_component_dbs, scan_status);

		/*
		 * Now we've completed the scan, update shared-memory. if we
		 * change anything, we return true.
		 */
		updated_probe_state = probePublishUpdate(scan_status);
#endif

		MemoryContextSwitchTo(oldContext);

		/* free any pallocs we made inside probeSegments() */
		MemoryContextReset(probeContext);
		cdb_component_dbs = NULL;

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

/*
 * Build a set of changes, based on our current state, and the probe results.
 */
static bool
probePublishUpdate(uint8 *probe_results)
{
	bool update_found = false;
	int i;

	/* preprocess probe results to decide what is the current segment state */
	FtsPreprocessProbeResultsFilerep(cdb_component_dbs, probe_results);

	for (i = 0; i < cdb_component_dbs->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *segInfo = &cdb_component_dbs->segment_db_info[i];

		/* if we've gotten a pause or shutdown request, we ignore our probe results. */
		if (!FtsIsActive())
		{
			return false;
		}

		/* we check segments in pairs of primary-mirror */
		if (!SEGMENT_IS_ACTIVE_PRIMARY(segInfo))
		{
			continue;
		}

		CdbComponentDatabaseInfo *primary = segInfo;
		CdbComponentDatabaseInfo *mirror = FtsGetPeerSegment(segInfo->segindex, segInfo->dbid);

		Assert(mirror != NULL);

		/* changes required for primary and mirror */
		FtsSegmentStatusChange changes[2];

		uint32 stateOld = 0;
		uint32 stateNew = 0;

		bool isPrimaryAlive = PROBE_IS_ALIVE(primary);
		bool isMirrorAlive = PROBE_IS_ALIVE(mirror);

		/* get transition type */
		uint32 trans = getTransition(isPrimaryAlive, isMirrorAlive);

		if (gp_log_fts > GPVARS_VERBOSITY_VERBOSE)
		{
			elog(LOG, "FTS: primary found %s, mirror found %s, transition %d.",
				 (isPrimaryAlive ? "alive" : "dead"), (isMirrorAlive ? "alive" : "dead"), trans);
		}

		if (trans == TRANS_D_D)
		{
			elog(LOG, "FTS: detected double failure for content=%d, primary (dbid=%d), mirror (dbid=%d).",
			     primary->segindex, primary->dbid, mirror->dbid);
		}

			/* get current state */
		stateOld = FtsGetPairStateFilerep(primary, mirror);

		/* get new state */
		stateNew = transition(stateOld, trans, primary, mirror, &changes[0], &changes[1]);

		/* check if transition is required */
		if (stateNew != stateOld)
		{
			update_found = true;
			updateConfiguration(changes, ARRAY_SIZE(changes));
		}
	}

	if (gp_log_fts >= GPVARS_VERBOSITY_VERBOSE)
	{
		elog(LOG, "FTS: probe result processing is complete.");
	}

	return update_found;
}


/*
 * Build struct with segment changes
 */
static void
buildSegmentStateChange(CdbComponentDatabaseInfo *segInfo, FtsSegmentStatusChange *change, uint8 statusNew)
{
	change->dbid = segInfo->dbid;
	change->segindex = segInfo->segindex;
	change->oldStatus = ftsProbeInfo->fts_status[segInfo->dbid];
	change->newStatus = statusNew;
}

/*
 * get transition type - derived from probed primary/mirror state
 */
static uint32
getTransition(bool isPrimaryAlive, bool isMirrorAlive)
{
	uint32 state = (isPrimaryAlive ? 2 : 0) + (isMirrorAlive ? 1 : 0);

	switch (state)
	{
		case (0):
			/* primary and mirror dead */
			return TRANS_D_D;
		case (1):
			/* primary dead, mirror alive */
			return TRANS_D_U;
		case (2):
			/* primary alive, mirror dead */
			return TRANS_U_D;
		case (3):
			/* primary and mirror alive */
			return TRANS_U_U;
		default:
			Assert(!"Invalid transition for FTS state machine");
			return 0;
	}
}


/*
 * find new state for primary and mirror
 */
static uint32
transition
	(
	uint32 stateOld,
	uint32 trans,
	CdbComponentDatabaseInfo *primary,
    CdbComponentDatabaseInfo *mirror,
    FtsSegmentStatusChange *changesPrimary,
    FtsSegmentStatusChange *changesMirror
    )
{
	Assert(IS_VALID_TRANSITION(trans));

	/* reset changes */
	memset(changesPrimary, 0, sizeof(*changesPrimary));
	memset(changesMirror, 0, sizeof(*changesMirror));

	uint32 stateNew = stateOld;

	/* in case of a double failure we don't do anything */
	if (trans == TRANS_D_D)
	{
		return stateOld;
	}

	/* get new state for primary and mirror */
	stateNew = FtsTransitionFilerep(stateOld, trans);

	/* check if transition is required */
	if (stateNew != stateOld)
	{
		FtsSegmentPairState pairState;
		memset(&pairState, 0, sizeof(pairState));
		pairState.primary = primary;
		pairState.mirror = mirror;
		pairState.stateNew = stateNew;
		pairState.statePrimary = 0;
		pairState.stateMirror = 0;

		if (gp_log_fts >= GPVARS_VERBOSITY_DEBUG)
		{
			elog(LOG, "FTS: state machine transition from %d to %d.", stateOld, stateNew);
		}

		FtsResolveStateFilerep(&pairState);

		buildSegmentStateChange(primary, changesPrimary, pairState.statePrimary);
		buildSegmentStateChange(mirror, changesMirror, pairState.stateMirror);

		FtsDumpChanges(changesPrimary, 1);
		FtsDumpChanges(changesMirror, 1);
	}

	return stateNew;
}


/*
 * Apply requested segment transitions
 */
static void
updateConfiguration(FtsSegmentStatusChange *changes, int changeEntries)
{
	Assert(changes != NULL);

	CdbComponentDatabaseInfo *entryDB = &cdb_component_dbs->entry_db_info[0];

	if (entryDB->dbid != GpIdentity.dbid)
	{
		if (gp_log_fts >= GPVARS_VERBOSITY_DEBUG)
		{
			elog(LOG, "FTS: advancing to second entry-db.");
		}
		entryDB = entryDB + 1;
	}

	/* if we've gotten a pause or shutdown request, we ignore our probe results. */
	if (!FtsIsActive())
	{
		return;
	}

	/* update segment configuration */
	bool commit = probeUpdateConfig(changes, changeEntries);

	if (commit)
		FtsFailoverFilerep(changes, changeEntries);

	if (gp_log_fts >= GPVARS_VERBOSITY_VERBOSE)
	{
		elog(LOG, "FTS: finished segment modifications.");
	}
}

/*
 * update segment configuration in catalog and shared memory
 */
static bool
probeUpdateConfig(FtsSegmentStatusChange *changes, int changeCount)
{
	Relation configrel;
	Relation histrel;
	SysScanDesc sscan;
	ScanKeyData scankey;
	HeapTuple configtuple;
	HeapTuple newtuple;
	HeapTuple histtuple;
	Datum configvals[Natts_gp_segment_configuration];
	bool confignulls[Natts_gp_segment_configuration] = { false };
	bool repls[Natts_gp_segment_configuration] = { false };
	Datum histvals[Natts_gp_configuration_history];
	bool histnulls[Natts_gp_configuration_history] = { false };
	bool valid;
	bool primary;
	bool changelogging;
	int i;
	char desc[SQL_CMD_BUF_SIZE];

	/*
	 * Commit/abort transaction below will destroy
	 * CurrentResourceOwner.  We need it for catalog reads.
	 */
	ResourceOwner save = CurrentResourceOwner;
	StartTransactionCommand();
	GetTransactionSnapshot();
	elog(LOG, "probeUpdateConfig called for %d changes", changeCount);

	histrel = heap_open(GpConfigHistoryRelationId,
						RowExclusiveLock);
	configrel = heap_open(GpSegmentConfigRelationId,
						  RowExclusiveLock);

	for (i = 0; i < changeCount; i++)
	{
		FtsSegmentStatusChange *change = &changes[i];
		valid   = (changes[i].newStatus & FTS_STATUS_ALIVE);
		primary = (changes[i].newStatus & FTS_STATUS_PRIMARY);
		changelogging = (changes[i].newStatus & FTS_STATUS_CHANGELOGGING);

		if (changelogging)
		{
			Assert(primary && valid);
		}

		Assert((valid || !primary) && "Primary cannot be down");

		/*
		 * Insert new tuple into gp_configuration_history catalog.
		 */
		histvals[Anum_gp_configuration_history_time-1] =
				TimestampTzGetDatum(GetCurrentTimestamp());
		histvals[Anum_gp_configuration_history_dbid-1] =
				Int16GetDatum(changes[i].dbid);
		snprintf(desc, sizeof(desc),
				 "FTS: content %d fault marking status %s%s role %c",
				 change->segindex, valid ? "UP" : "DOWN",
				 (changelogging) ? " mode: change-tracking" : "",
				 primary ? GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY : GP_SEGMENT_CONFIGURATION_ROLE_MIRROR);
		histvals[Anum_gp_configuration_history_desc-1] =
					CStringGetTextDatum(desc);

		histtuple = heap_form_tuple(RelationGetDescr(histrel), histvals, histnulls);
		simple_heap_insert(histrel, histtuple);
		CatalogUpdateIndexes(histrel, histtuple);

		/*
		 * Find and update gp_segment_configuration tuple.
		 */
		ScanKeyInit(&scankey,
					Anum_gp_segment_configuration_dbid,
					BTEqualStrategyNumber, F_INT2EQ,
					Int16GetDatum(changes[i].dbid));
		sscan = systable_beginscan(configrel, GpSegmentConfigDbidIndexId,
								   true, SnapshotNow, 1, &scankey);
		configtuple = systable_getnext(sscan);
		if (!HeapTupleIsValid(configtuple))
		{
			elog(ERROR, "FTS cannot find dbid=%d in %s", changes[i].dbid,
				 RelationGetRelationName(configrel));
		}
		configvals[Anum_gp_segment_configuration_role-1] =
				CharGetDatum(primary ? GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY : GP_SEGMENT_CONFIGURATION_ROLE_MIRROR);
		repls[Anum_gp_segment_configuration_role-1] = true;
		configvals[Anum_gp_segment_configuration_status-1] =
				CharGetDatum(valid ? GP_SEGMENT_CONFIGURATION_STATUS_UP : GP_SEGMENT_CONFIGURATION_STATUS_DOWN);
		repls[Anum_gp_segment_configuration_status-1] = true;
		if (changelogging)
		{
			configvals[Anum_gp_segment_configuration_mode-1] =
					CharGetDatum(GP_SEGMENT_CONFIGURATION_MODE_CHANGETRACKING);
		}
		repls[Anum_gp_segment_configuration_mode-1] = changelogging;

		newtuple = heap_modify_tuple(configtuple, RelationGetDescr(configrel),
									 configvals, confignulls, repls);
		simple_heap_update(configrel, &configtuple->t_self, newtuple);
		CatalogUpdateIndexes(configrel, newtuple);

		systable_endscan(sscan);
		pfree(newtuple);
		/*
		 * Update shared memory
		 */
		ftsProbeInfo->fts_status[changes[i].dbid] = changes[i].newStatus;
	}
	heap_close(histrel, RowExclusiveLock);
	heap_close(configrel, RowExclusiveLock);

	SIMPLE_FAULT_INJECTOR(FtsWaitForShutdown);
	/*
	 * Do not block shutdown.  We will always get a change to update
	 * gp_segment_configuration in subsequent probes upon database
	 * restart.
	 */
	if (shutdown_requested)
	{
		elog(LOG, "Shutdown in progress, ignoring FTS prober updates.");
		return false;
	}
	CommitTransactionCommand();
	CurrentResourceOwner = save;
	return true;
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

/**
 * Marks the given db as in-sync in the segment configuration.
 */
void
FtsMarkSegmentsInSync(CdbComponentDatabaseInfo *primary, CdbComponentDatabaseInfo *mirror)
{
	if (!FTS_STATUS_ISALIVE(primary->dbid, ftsProbeInfo->fts_status) ||
	    !FTS_STATUS_ISALIVE(mirror->dbid, ftsProbeInfo->fts_status) ||
	    !FTS_STATUS_ISPRIMARY(primary->dbid, ftsProbeInfo->fts_status) ||
 	    FTS_STATUS_ISPRIMARY(mirror->dbid, ftsProbeInfo->fts_status) ||
	    FTS_STATUS_IS_SYNCED(primary->dbid, ftsProbeInfo->fts_status) ||
	    FTS_STATUS_IS_SYNCED(mirror->dbid, ftsProbeInfo->fts_status) ||
	    FTS_STATUS_IS_CHANGELOGGING(primary->dbid, ftsProbeInfo->fts_status) ||
	    FTS_STATUS_IS_CHANGELOGGING(mirror->dbid, ftsProbeInfo->fts_status))
	{
		FtsRequestPostmasterShutdown(primary, mirror);
	}

	if (ftsProbeInfo->fts_pauseProbes)
	{
		return;
	}

	uint8	segStatus=0;
	Relation configrel;
	Relation histrel;
	ScanKeyData scankey;
	SysScanDesc sscan;
	HeapTuple configtuple;
	HeapTuple newtuple;
	HeapTuple histtuple;
	Datum configvals[Natts_gp_segment_configuration];
	bool confignulls[Natts_gp_segment_configuration] = { false };
	bool repls[Natts_gp_segment_configuration] = { false };
	Datum histvals[Natts_gp_configuration_history];
	bool histnulls[Natts_gp_configuration_history] = { false };
	char *desc = "FTS: changed segment to insync from resync.";
	/*
	 * Commit/abort transaction below will destroy
	 * CurrentResourceOwner.  We need it for catalog reads.
	 */
	ResourceOwner save = CurrentResourceOwner;
	StartTransactionCommand();
	GetTransactionSnapshot();

	/* update primary */
	segStatus = ftsProbeInfo->fts_status[primary->dbid];
	segStatus |= FTS_STATUS_SYNCHRONIZED;
	ftsProbeInfo->fts_status[primary->dbid] = segStatus;

	/* update mirror */
	segStatus = ftsProbeInfo->fts_status[mirror->dbid];
	segStatus |= FTS_STATUS_SYNCHRONIZED;
	ftsProbeInfo->fts_status[mirror->dbid] = segStatus;

	histrel = heap_open(GpConfigHistoryRelationId,
						RowExclusiveLock);
	configrel = heap_open(GpSegmentConfigRelationId,
						  RowExclusiveLock);

	/* update gp_segment_configuration to insync */
	ScanKeyInit(&scankey,
				Anum_gp_segment_configuration_dbid,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(primary->dbid));
	sscan = systable_beginscan(configrel, GpSegmentConfigDbidIndexId,
							   true, SnapshotNow, 1, &scankey);
	configtuple = systable_getnext(sscan);
	if (!HeapTupleIsValid(configtuple))
	{
		elog(ERROR,"FTS cannot find dbid (%d, %d) in %s", primary->dbid,
			 mirror->dbid, RelationGetRelationName(configrel));
	}
	configvals[Anum_gp_segment_configuration_mode-1] = CharGetDatum(GP_SEGMENT_CONFIGURATION_MODE_INSYNC);
	repls[Anum_gp_segment_configuration_mode-1] = true;
	newtuple = heap_modify_tuple(configtuple, RelationGetDescr(configrel),
								 configvals, confignulls, repls);
	simple_heap_update(configrel, &configtuple->t_self, newtuple);
	CatalogUpdateIndexes(configrel, newtuple);

	systable_endscan(sscan);

	ScanKeyInit(&scankey,
				Anum_gp_segment_configuration_dbid,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(mirror->dbid));
	sscan = systable_beginscan(configrel, GpSegmentConfigDbidIndexId,
							   true, SnapshotNow, 1, &scankey);
	configtuple = systable_getnext(sscan);
	if (!HeapTupleIsValid(configtuple))
	{
		elog(ERROR,"FTS cannot find dbid (%d, %d) in %s", primary->dbid,
			 mirror->dbid, RelationGetRelationName(configrel));
	}
	newtuple = heap_modify_tuple(configtuple, RelationGetDescr(configrel),
								 configvals, confignulls, repls);
	simple_heap_update(configrel, &configtuple->t_self, newtuple);
	CatalogUpdateIndexes(configrel, newtuple);

	systable_endscan(sscan);

	/* update configuration history */
	histvals[Anum_gp_configuration_history_time-1] =
			TimestampTzGetDatum(GetCurrentTimestamp());
	histvals[Anum_gp_configuration_history_dbid-1] =
			Int16GetDatum(primary->dbid);
	histvals[Anum_gp_configuration_history_desc-1] =
				CStringGetTextDatum(desc);
	histtuple = heap_form_tuple(RelationGetDescr(histrel), histvals, histnulls);
	simple_heap_insert(histrel, histtuple);
	CatalogUpdateIndexes(histrel, histtuple);

	histvals[Anum_gp_configuration_history_dbid-1] =
			Int16GetDatum(mirror->dbid);
	histtuple = heap_form_tuple(RelationGetDescr(histrel), histvals, histnulls);
	simple_heap_insert(histrel, histtuple);
	CatalogUpdateIndexes(histrel, histtuple);
	ereport(LOG,
			(errmsg("FTS: resynchronization of mirror (dbid=%d, content=%d) on %s:%d has completed.",
					mirror->dbid, mirror->segindex, mirror->address, mirror->port ),
			 errSendAlert(true)));

	heap_close(histrel, RowExclusiveLock);
	heap_close(configrel, RowExclusiveLock);
	/*
	 * Do not block shutdown.  We will always get a change to update
	 * gp_segment_configuration in subsequent probes upon database
	 * restart.
	 */
	if (shutdown_requested)
	{
		elog(LOG, "Shutdown in progress, ignoring FTS prober updates.");
		return;
	}
	CommitTransactionCommand();
	CurrentResourceOwner = save;
}

/*
 * Get peer segment descriptor
 */
CdbComponentDatabaseInfo *FtsGetPeerSegment(int content, int dbid)
{
	int i;

	for (i=0; i < cdb_component_dbs->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *segInfo = &cdb_component_dbs->segment_db_info[i];

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
