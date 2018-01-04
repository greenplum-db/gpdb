/*-------------------------------------------------------------------------
 *
 * cdbfts.c
 *	  Provides fault tolerance service routines for mpp.
 *
 * Portions Copyright (c) 2003-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbfts.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "miscadmin.h"
#include "libpq-fe.h"
#include "libpq-int.h"
#include "utils/memutils.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbconn.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbdisp_query.h"
#include "access/xact.h"
#include "cdb/cdbfts.h"
#include "cdb/cdbtm.h"
#include "libpq/libpq-be.h"
#include "commands/dbcommands.h"
#include "storage/proc.h"

#include "executor/spi.h"

#include "postmaster/fts.h"
#include "postmaster/primary_mirror_mode.h"
#include "utils/faultinjection.h"

#include "utils/fmgroids.h"
#include "catalog/pg_authid.h"

/* segment id for the master */
#define MASTER_SEGMENT_ID -1

FtsProbeInfo *ftsProbeInfo = NULL;	/* Probe process updates this structure */
volatile bool *ftsEnabled;
volatile bool *ftsShutdownMaster;
static LWLockId ftsControlLock;

static volatile bool *ftsReadOnlyFlag;
static volatile bool *ftsAdminRequestedRO;

static bool local_fts_status_initialized = false;
static uint64 local_fts_statusVersion;

/*
 * get fts share memory size
 */
int
FtsShmemSize(void)
{
	/*
	 * this shared memory block doesn't even need to *exist* on the QEs!
	 */
	if ((Gp_role != GP_ROLE_DISPATCH) && (Gp_role != GP_ROLE_UTILITY))
		return 0;

	return MAXALIGN(sizeof(FtsControlBlock));
}

void
FtsShmemInit(void)
{
	bool		found;
	FtsControlBlock *shared;

	shared = (FtsControlBlock *) ShmemInitStruct("Fault Tolerance manager", FtsShmemSize(), &found);
	if (!shared)
		elog(FATAL, "FTS: could not initialize fault tolerance manager share memory");

	/* Initialize locks and shared memory area */

	ftsEnabled = &shared->ftsEnabled;
	ftsShutdownMaster = &shared->ftsShutdownMaster;
	ftsControlLock = shared->ControlLock;

	ftsReadOnlyFlag = &shared->ftsReadOnlyFlag; /* global RO state */

	ftsAdminRequestedRO = &shared->ftsAdminRequestedRO; /* Admin request --
														 * guc-controlled RO
														 * state */

	ftsProbeInfo = &shared->fts_probe_info;

	if (!IsUnderPostmaster)
	{
		shared->ControlLock = LWLockAssign();
		ftsControlLock = shared->ControlLock;

		/* initialize */
		shared->ftsReadOnlyFlag = gp_set_read_only;
		shared->ftsAdminRequestedRO = gp_set_read_only;

		shared->fts_probe_info.fts_probePid = 0;
		shared->fts_probe_info.fts_pauseProbes = false;
		shared->fts_probe_info.fts_discardResults = false;
		shared->fts_probe_info.fts_statusVersion = 0;
		shared->fts_probe_info.fts_status_initialized = false;

		shared->ftsEnabled = true;	/* ??? */
		shared->ftsShutdownMaster = false;
	}
}

void
ftsLock(void)
{
	LWLockAcquire(ftsControlLock, LW_EXCLUSIVE);
}

void
ftsUnlock(void)
{
	LWLockRelease(ftsControlLock);
}

void
FtsNotifyProber(void)
{
	Assert(Gp_role == GP_ROLE_DISPATCH);

	if (ftsProbeInfo->fts_probePid == 0)
		return;

	/*
	 * This is a full-scan request. We set the request-flag == to the bitmap
	 * version flag. When the version has been bumped, we know that the
	 * request has been filled.
	 */
	ftsProbeInfo->fts_probeScanRequested = ftsProbeInfo->fts_statusVersion;

	/* signal fts-probe */
	kill(ftsProbeInfo->fts_probePid, SIGINT);

	/* sit and spin */
	while (ftsProbeInfo->fts_probeScanRequested == ftsProbeInfo->fts_statusVersion)
	{
		pg_usleep(50000);

		CHECK_FOR_INTERRUPTS();
	}
}


/*
 * Check if master needs to shut down
 */
bool
FtsMasterShutdownRequested()
{
	return *ftsShutdownMaster;
}


/*
 * Set flag indicating that master needs to shut down
 */
void
FtsRequestMasterShutdown()
{
#ifdef USE_ASSERT_CHECKING
	Assert(!*ftsShutdownMaster);

	PrimaryMirrorMode pm_mode;

	getPrimaryMirrorStatusCodes(&pm_mode, NULL, NULL, NULL);
	Assert(pm_mode == PMModeMaster);
#endif							/* USE_ASSERT_CHECKING */

	*ftsShutdownMaster = true;
}


/*
 * Test-Connection: This is called from the threaded context inside the
 * dispatcher: ONLY CALL THREADSAFE FUNCTIONS -- elog() is NOT threadsafe.
 */
bool
FtsTestConnection(CdbComponentDatabaseInfo *failedDBInfo, bool fullScan)
{
	/* master is always reported as alive */
	if (failedDBInfo->segindex == MASTER_SEGMENT_ID)
	{
		return true;
	}

	if (!fullScan)
	{
		/*
		 * if fullscan not requested, caller is just trying to optimize on
		 * cached version but if we haven't populated yet the fts_status, we
		 * don't have one and hence just return positively back. This is
		 * mainly to avoid queries incorrectly failing just after QD restarts
		 * if FTS process is yet to start and complete initializing the
		 * fts_status. We shouldn't be checking against uninitialzed variable.
		 */
		if (ftsProbeInfo->fts_status_initialized)
			return FTS_STATUS_IS_UP(ftsProbeInfo->fts_status[failedDBInfo->dbid]);

		return true;
	}

	FtsNotifyProber();

	Assert(ftsProbeInfo->fts_status_initialized);

	return FTS_STATUS_IS_UP(ftsProbeInfo->fts_status[failedDBInfo->dbid]);
}

/*
 * Re-Configure the system: if someone has noticed that the status
 * version has been updated, they call this to verify that they've got
 * the right configuration.
 *
 * NOTE: This *always* destroys gangs. And also attempts to inform the
 * fault-prober to do a full scan.
 */
void
FtsReConfigureMPP(bool create_new_gangs)
{
	/* need to scan to pick up the latest view */
	FtsNotifyProber();
	local_fts_statusVersion = ftsProbeInfo->fts_statusVersion;

	ereport(LOG, (errmsg_internal("FTS: reconfiguration is in progress"),
				  errSendAlert(true)));
	DisconnectAndDestroyAllGangs(true);

	/* Caller should throw an error. */
	return;
}

void
FtsHandleNetFailure(SegmentDatabaseDescriptor **segDB, int numOfFailed)
{
	elog(LOG, "FtsHandleNetFailure: numOfFailed %d", numOfFailed);

	FtsReConfigureMPP(true);

	ereport(ERROR, (errmsg_internal("MPP detected %d segment failures, system is reconnected", numOfFailed),
					errSendAlert(true)));
}

/*
 * Check if any segment DB is down.
 *
 * returns true if any segment DB is down.
 */
bool
FtsTestSegmentDBIsDown(SegmentDatabaseDescriptor *segdbDesc, int size)
{
	int			i = 0;
	bool		forceRescan = true;

	Assert(isFTSEnabled());

	for (i = 0; i < size; i++)
	{
		CdbComponentDatabaseInfo *segInfo = segdbDesc[i].segment_database_info;

		elog(DEBUG2, "FtsTestSegmentDBIsDown: looking for real fault on segment dbid %d", segInfo->dbid);

		if (!FtsTestConnection(segInfo, forceRescan))
		{
			ereport(LOG, (errmsg_internal("FTS: found fault with segment dbid %d. "
										  "Reconfiguration is in progress", segInfo->dbid)));
			return true;
		}

		/* only force the rescan on the first call. */
		forceRescan = false;
	}

	return false;
}


void
FtsCondSetTxnReadOnly(bool *XactFlag)
{
	if (!isFTSEnabled())
		return;

	if (*ftsReadOnlyFlag && Gp_role != GP_ROLE_UTILITY)
		*XactFlag = true;
}

bool
verifyFtsSyncCount(void)
{
	if (ftsProbeInfo->fts_probePid == 0)
		return true;

	if (!local_fts_status_initialized)
	{
		local_fts_status_initialized = true;
		local_fts_statusVersion = ftsProbeInfo->fts_statusVersion;

		return true;
	}

	return (local_fts_statusVersion == ftsProbeInfo->fts_statusVersion);
}

bool
isFtsReadOnlySet(void)
{
	return *ftsReadOnlyFlag;
}

uint64
getFtsVersion(void)
{
	return ftsProbeInfo->fts_statusVersion;
}
