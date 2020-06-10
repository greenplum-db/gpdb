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

/* These are always necessary for a bgworker */
#include "miscadmin.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/shmem.h"

#include "access/xact.h"
#include "catalog/indexing.h"
#include "libpq/pqsignal.h"
#include "cdb/cdbvars.h"
#include "libpq-int.h"
#include "cdb/cdbfts.h"
#include "postmaster/fts.h"
#include "postmaster/ftsprobe.h"
#include "postmaster/postmaster.h"
#include "utils/builtins.h"
#include "utils/faultinjector.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"

#include "catalog/gp_configuration_history.h"
#include "catalog/gp_segment_config.h"

#include "tcop/tcopprot.h" /* quickdie() */

bool am_ftsprobe = false;
bool am_ftshandler = false;

/*
 * STATIC VARIABLES
 */
volatile pid_t *shmFtsProbePID = NULL;
static bool skip_fts_probe = false;

static volatile bool probe_requested = false;
static volatile sig_atomic_t got_SIGHUP = false;

/*
 * FUNCTION PROTOTYPES
 */
static void FtsLoop(void);

static CdbComponentDatabases *readCdbComponentInfoAndUpdateStatus(MemoryContext);

/*=========================================================================
 * HELPER FUNCTIONS
 */
/* SIGHUP: set flag to reload config file */
static void
sigHupHandler(SIGNAL_ARGS)
{
	got_SIGHUP = true;

	if (MyProc)
		SetLatch(MyLatch);
}

/* SIGINT: set flag to indicate a FTS scan is requested */
static void
sigIntHandler(SIGNAL_ARGS)
{
	probe_requested = true;

	if (MyProc)
		SetLatch(MyLatch);
}

pid_t
FtsProbePID(void)
{
	return *shmFtsProbePID;
}

bool
FtsProbeStartRule(Datum main_arg)
{
	return (Gp_role == GP_ROLE_DISPATCH);
}

/*
 * FtsProbeMain
 */
void
FtsProbeMain(Datum main_arg)
{
	*shmFtsProbePID = MyProcPid;
	am_ftsprobe = true;

	/*
	 * reread postgresql.conf if requested
	 */
	pqsignal(SIGHUP, sigHupHandler);
	pqsignal(SIGINT, sigIntHandler);

	/* We're now ready to receive signals */
	BackgroundWorkerUnblockSignals();

	/* Connect to our database */
	BackgroundWorkerInitializeConnection(DB_FOR_COMMON_ACCESS, NULL);

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
	CdbComponentDatabases *cdbs = cdbcomponent_getCdbComponents();

	for (i=0; i < cdbs->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *segInfo = &cdbs->segment_db_info[i];
		uint8	segStatus = 0;

		if (!SEGMENT_IS_ALIVE(segInfo))
			FTS_STATUS_SET_DOWN(segStatus);

		ftsProbeInfo->status[segInfo->config->dbid] = segStatus;
	}

	/*
	 * Initialize fts_stausVersion after populating the config details in
	 * shared memory for the first time after FTS startup.
	 */
	if (ftsProbeInfo->status_version == 0)
	{
		ftsProbeInfo->status_version++;
		writeGpSegConfigToFTSFiles();
	}

	return cdbs;
}

void
probeWalRepUpdateConfig(int16 dbid, int16 segindex, char role,
						bool IsSegmentAlive, bool IsInSync)
{
	AssertImply(IsInSync, IsSegmentAlive);

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

		SIMPLE_FAULT_INJECTOR("fts_update_config");

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
								   true, NULL, 1, &scankey);

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

static
void FtsLoop()
{
	bool	updated_probe_state;
	MemoryContext probeContext = NULL, oldContext = NULL;
	time_t elapsed,	probe_start_time, timeout;
	CdbComponentDatabases *cdbs = NULL;

	probeContext = AllocSetContextCreate(TopMemoryContext,
										 "FtsProbeMemCtxt",
										 ALLOCSET_DEFAULT_INITSIZE,	/* always have some memory */
										 ALLOCSET_DEFAULT_INITSIZE,
										 ALLOCSET_DEFAULT_MAXSIZE);

	while (true)
	{
		bool		has_mirrors;
		int			rc;

		if (got_SIGHUP)
		{
			got_SIGHUP = false;
			ProcessConfigFile(PGC_SIGHUP);
		}

		CHECK_FOR_INTERRUPTS();

		SIMPLE_FAULT_INJECTOR("ftsLoop_before_probe");

		probe_start_time = time(NULL);

		SpinLockAcquire(&ftsProbeInfo->lock);
		ftsProbeInfo->start_count++;
		SpinLockRelease(&ftsProbeInfo->lock);

		/* Need a transaction to access the catalogs */
		StartTransactionCommand();

		cdbs = readCdbComponentInfoAndUpdateStatus(probeContext);

		/* Check here gp_segment_configuration if has mirror's */
		has_mirrors = gp_segment_config_has_mirrors();

		/* close the transaction we started above */
		CommitTransactionCommand();

		/* Reset this as we are performing the probe */
		probe_requested = false;
		skip_fts_probe = false;

#ifdef FAULT_INJECTOR
		if (SIMPLE_FAULT_INJECTOR("fts_probe") == FaultInjectorTypeSkip)
			skip_fts_probe = true;
#endif

		if (skip_fts_probe || !has_mirrors)
		{
			elogif(gp_log_fts >= GPVARS_VERBOSITY_VERBOSE, LOG,
				   "skipping FTS probes due to %s",
				   !has_mirrors ? "no mirrors" : "fts_probe fault");

		}
		else
		{
			elogif(gp_log_fts == GPVARS_VERBOSITY_DEBUG, LOG,
				   "FTS: starting %s scan with %d segments and %d contents",
				   (probe_requested ? "full " : ""),
				   cdbs->total_segment_dbs,
				   cdbs->total_segments);
			/*
			 * We probe in a special context, some of the heap access
			 * stuff palloc()s internally
			 */
			oldContext = MemoryContextSwitchTo(probeContext);

			updated_probe_state = FtsWalRepMessageSegments(cdbs);

			MemoryContextSwitchTo(oldContext);

			/* free any pallocs we made inside probeSegments() */
			MemoryContextReset(probeContext);

			/* Bump the version if configuration was updated. */
			if (updated_probe_state)
			{
				/*
				 * File GPSEGCONFIGDUMPFILE under $PGDATA is used by other
				 * components to fetch latest gp_segment_configuration outside
				 * of a transaction. FTS update this file in the first probe
				 * and every probe which updated gp_segment_configuration.
				 */
				StartTransactionCommand();
				writeGpSegConfigToFTSFiles();
				CommitTransactionCommand();

				ftsProbeInfo->status_version++;
			}
		}

		/* free current components info and free ip addr caches */	
		cdbcomponent_destroyCdbComponents();

		SIMPLE_FAULT_INJECTOR("ftsLoop_after_probe");

		/* Notify any waiting backends about probe cycle completion. */
		SpinLockAcquire(&ftsProbeInfo->lock);
		ftsProbeInfo->done_count = ftsProbeInfo->start_count;
		SpinLockRelease(&ftsProbeInfo->lock);


		/* check if we need to sleep before starting next iteration */
		elapsed = time(NULL) - probe_start_time;
		timeout = elapsed >= gp_fts_probe_interval ? 0 : 
							gp_fts_probe_interval - elapsed;

		/*
		 * In above code we might update gp_segment_configuration and then wal
		 * is generated. While synchronizing wal to standby, we need to wait on
		 * MyLatch also in SyncRepWaitForLSN(). The set latch introduced by
		 * outside fts probe trigger (e.g. gp_request_fts_probe_scan() or
		 * FtsNotifyProber()) might be consumed by it so we do not WaitLatch()
		 * here with a long timout here else we may block for that long
		 * timeout, so we recheck probe_requested here before waitLatch().
		 */
		if (probe_requested)
			timeout = 0;

		rc = WaitLatch(&MyProc->procLatch,
					   WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
					   timeout * 1000L);

		SIMPLE_FAULT_INJECTOR("ftsLoop_after_latch");

		ResetLatch(&MyProc->procLatch);

		/* emergency bailout if postmaster has died */
		if (rc & WL_POSTMASTER_DEATH)
			proc_exit(1);
	} /* end server loop */

	return;
}

/*
 * Check if FTS is active
 */
bool
FtsIsActive(void)
{
	return !skip_fts_probe;
}
