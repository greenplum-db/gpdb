/*-------------------------------------------------------------------------
 *
 * cdbfts.c
 *	  Provides fault tolerance service routines for mpp.
 *
 * Copyright (c) 2003-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include <assert.h>

#include "miscadmin.h"
#include "gp-libpq-fe.h"
#include "gp-libpq-int.h"
#include "utils/memutils.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbconn.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbdisp.h"
#include "cdb/cdbgangmgr.h"
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

FtsProbeInfo *ftsProbeInfo = NULL; /* Probe process updates this structure */
volatile bool	*ftsEnabled;
volatile bool	*ftsShutdownMaster;
static LWLockId	ftsControlLock;

static volatile bool	*ftsReadOnlyFlag;
static volatile bool	*ftsAdminRequestedRO;

static bool		local_fts_status_initialized=false;
static uint64	local_fts_statusVersion;

/*
 * get fts share memory size
 */
int
FtsShmemSize(void)
{
	/*
	 * this shared memory block doesn't even need to *exist* on the
	 * QEs!
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

	shared = (FtsControlBlock *)ShmemInitStruct("Fault Tolerance manager", FtsShmemSize(), &found);
	if (!shared)
		elog(FATAL, "FTS: could not initialize fault tolerance manager share memory");

	/* Initialize locks and shared memory area */

	ftsEnabled = &shared->ftsEnabled;
	ftsShutdownMaster = &shared->ftsShutdownMaster;
	ftsControlLock = shared->ControlLock;

	ftsReadOnlyFlag = &shared->ftsReadOnlyFlag; /* global RO state */

	ftsAdminRequestedRO = &shared->ftsAdminRequestedRO; /* Admin request -- guc-controlled RO state */

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

		shared->ftsEnabled = true; /* ??? */
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
	/*
	 * This is a full-scan request. We set the request-flag == to the bitmap version flag.
	 * When the version has been bumped, we know that the request has been filled.
	 */
	ftsProbeInfo->fts_probeScanRequested = ftsProbeInfo->fts_statusVersion;

	/* signal fts-probe */
	if (ftsProbeInfo->fts_probePid)
		kill(ftsProbeInfo->fts_probePid, SIGINT);

	/* sit and spin */
	while (ftsProbeInfo->fts_probeScanRequested == ftsProbeInfo->fts_statusVersion)
	{
		struct timeval tv;

		tv.tv_usec = 50000;
		tv.tv_sec = 0;
		select(0, NULL, NULL, NULL, &tv); /* don't care about return value. */

		CHECK_FOR_INTERRUPTS();
	}
}


/*
 * Check if master needs to shut down
 */
bool FtsMasterShutdownRequested()
{
	return *ftsShutdownMaster;
}


/*
 * Set flag indicating that master needs to shut down
 */
void FtsRequestMasterShutdown()
{
#ifdef USE_ASSERT_CHECKING
	Assert(!*ftsShutdownMaster);

	PrimaryMirrorMode pm_mode;
	getPrimaryMirrorStatusCodes(&pm_mode, NULL, NULL, NULL);
	Assert(pm_mode == PMModeMaster);
#endif /*USE_ASSERT_CHECKING*/

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
		return FTS_STATUS_ISALIVE(failedDBInfo->dbid, ftsProbeInfo->fts_status);
	}

	FtsNotifyProber();

	return FTS_STATUS_ISALIVE(failedDBInfo->dbid, ftsProbeInfo->fts_status);
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
	GetGangMgr().detectFailedConnections();
	local_fts_statusVersion = ftsProbeInfo->fts_statusVersion;

	ereport(LOG, (errmsg_internal("FTS: reconfiguration is in progress"),
			errSendAlert(true)));
	GetGangMgr().disconnectAndDestroyAllGangs();

	/* Caller should throw an error. */
	return;
}

void
FtsHandleNetFailure(SegmentDatabaseDescriptor ** segDB, int numOfFailed)
{
	elog(LOG, "FtsHandleNetFailure: numOfFailed %d", numOfFailed);

	FtsReConfigureMPP(true);

	ereport(ERROR, (errmsg_internal("MPP detected %d segment failures, system is reconnected", numOfFailed),
			errSendAlert(true)));
}

/*
 * FtsHandleGangConnectionFailure is called by createGang during
 * creating connections return true if error need to be thrown
 */
bool
FtsHandleGangConnectionFailure(SegmentDatabaseDescriptor * segdbDesc, int size)
{
	int			i;
	bool		dtx_active;
	bool		reportError = false;
    bool		realFaultFound = false;
	bool		forceRescan=true;

    for (i = 0; i < size; i++)
    {
        if (PQstatus(segdbDesc[i].conn) != CONNECTION_OK)
        {
			CdbComponentDatabaseInfo *segInfo = segdbDesc[i].segment_database_info;

			elog(DEBUG2, "FtsHandleGangConnectionFailure: looking for real fault on segment dbid %d", segInfo->dbid);

			if (!FtsTestConnection(segInfo, forceRescan))
			{
				elog(DEBUG2, "found fault with segment dbid %d", segInfo->dbid);
				realFaultFound = true;

				/* that at least one fault exists is enough, for now */
				break;
			}
			forceRescan = false; /* only force the rescan on the first call. */
        }
    }

    if (!realFaultFound)
	{
		/* If we successfully tested the gang and didn't notice a
		 * failure, our caller must've seen some kind of transient
		 * failure when the gang was originally constructed ...  */
		elog(DEBUG2, "FtsHandleGangConnectionFailure: no real fault found!");
        return false;
	}

	if (!isFTSEnabled())
	{
		return false;
	}

	ereport(LOG, (errmsg_internal("FTS: reconfiguration is in progress")));

	forceRescan = true;
	for (i = 0; i < size; i++)
	{
		CdbComponentDatabaseInfo *segInfo = segdbDesc[i].segment_database_info;

		if (PQstatus(segdbDesc[i].conn) != CONNECTION_OK)
		{
			if (!FtsTestConnection(segInfo, forceRescan))
			{
				ereport(WARNING, (errmsg_internal("FTS: found bad segment with dbid %d", segInfo->dbid),
						errSendAlert(true)));
				/* probe process has already marked segment down. */
			}
			forceRescan = false; /* only force rescan on first call. */
		}
	}

	if (GetGangMgr().gangsExist())
	{
		reportError = true;
		GetGangMgr().disconnectAndDestroyAllGangs();
	}

	/*
	 * KLUDGE: Do not error out if we are attempting a DTM protocol retry
	 */
	if (DistributedTransactionContext == DTX_CONTEXT_QD_RETRY_PHASE_2)
	{
		return false;
	}

	/* is there a transaction active ? */
	dtx_active = isCurrentDtxActive();

    /* When the error is raised, it will abort the current DTM transaction */
	if (dtx_active)
	{
		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "FtsHandleGangConnectionFailure found an active DTM transaction (returning true).");
		return true;
	}

	/*
	 * error out if this sets read only flag, at this stage the read only
	 * transaction checking has passed, so error out, but do not error out if
	 * tm is in recovery
	 */
	if ((*ftsReadOnlyFlag && !isTMInRecovery()) || reportError)
		return true;

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "FtsHandleGangConnectionFailure returning false.");

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
