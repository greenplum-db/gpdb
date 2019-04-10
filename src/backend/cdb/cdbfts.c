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
#include "storage/pmsignal.h"
#include "storage/proc.h"

#include "executor/spi.h"

#include "postmaster/fts.h"
#include "utils/faultinjection.h"

#include "utils/fmgroids.h"
#include "catalog/pg_authid.h"

/* segment id for the master */
#define MASTER_SEGMENT_ID -1

volatile FtsProbeInfo *ftsProbeInfo = NULL;	/* Probe process updates this structure */
static LWLockId ftsControlLock;

extern volatile bool *pm_launch_walreceiver;

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
	ftsControlLock = shared->ControlLock;
	ftsProbeInfo = &shared->fts_probe_info;
	pm_launch_walreceiver = &shared->pm_launch_walreceiver;

	if (!IsUnderPostmaster)
	{
		shared->ControlLock = LWLockAssign();
		ftsControlLock = shared->ControlLock;

		shared->fts_probe_info.fts_statusVersion = 0;
		shared->fts_probe_info.inProgress = false;
		shared->pm_launch_walreceiver = false;
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

static void
sleep_until_current_probe_is_finished(void)
{
	while(ftsProbeInfo->inProgress)
	{
		pg_usleep(50000);
		CHECK_FOR_INTERRUPTS();
	}
}

static void
mark_probe_in_progress(void)
{
	ftsProbeInfo->inProgress = true;
}

static void
mark_probe_finished(void)
{
	ftsProbeInfo->inProgress = false;
}

/* sit and spin */
static void
sleep_until_fts_wakens(const uint8 currentTick)
{
	while (currentTick == ftsProbeInfo->probeTick)
	{
		pg_usleep(50000);
		CHECK_FOR_INTERRUPTS();
	}
}

void
FtsNotifyProber(bool wait_for_current_probe_to_finish)
{
	Assert(Gp_role == GP_ROLE_DISPATCH);
	
	if (wait_for_current_probe_to_finish)
		sleep_until_current_probe_is_finished();

	mark_probe_in_progress();
	
	uint8 probeTick = ftsProbeInfo->probeTick;

	/* signal fts-probe */
	SendPostmasterSignal(PMSIGNAL_WAKEN_FTS);

	sleep_until_fts_wakens(probeTick);

	mark_probe_finished();
}

/*
 * Test-Connection: This is called from the threaded context inside the
 * dispatcher: ONLY CALL THREADSAFE FUNCTIONS -- elog() is NOT threadsafe.
 */
bool
FtsIsSegmentDown(CdbComponentDatabaseInfo *dBInfo)
{
	/* master is always reported as alive */
	if (dBInfo->segindex == MASTER_SEGMENT_ID)
		return false;

	return FTS_STATUS_IS_DOWN(ftsProbeInfo->fts_status[dBInfo->dbid]);
}

/*
 * Check if any segment DB is down.
 *
 * returns true if any segment DB is down.
 */
bool
FtsTestSegmentDBIsDown(SegmentDatabaseDescriptor **segdbDesc, int size)
{
	int			i = 0;

	for (i = 0; i < size; i++)
	{
		CdbComponentDatabaseInfo *segInfo = segdbDesc[i]->segment_database_info;

		elog(DEBUG2, "FtsTestSegmentDBIsDown: looking for real fault on segment dbid %d", (int) segInfo->dbid);

		if (FtsIsSegmentDown(segInfo))
		{
			ereport(LOG, (errmsg_internal("FTS: found fault with segment dbid %d. "
										  "Reconfiguration is in progress", (int) segInfo->dbid)));
			return true;
		}
	}

	return false;
}

uint8
getFtsVersion(void)
{
	return ftsProbeInfo->fts_statusVersion;
}
