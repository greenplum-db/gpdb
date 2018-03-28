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

	if (!IsUnderPostmaster)
	{
		shared->ControlLock = LWLockAssign();
		ftsControlLock = shared->ControlLock;

		shared->fts_probe_info.fts_statusVersion = 0;
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
	uint8 probeTick = ftsProbeInfo->probeTick;

	/* signal fts-probe */
	SendPostmasterSignal(PMSIGNAL_WAKEN_FTS);

	/* sit and spin */
	while (probeTick == ftsProbeInfo->probeTick)
	{
		pg_usleep(50000);
		CHECK_FOR_INTERRUPTS();
	}
}

/*
 * Test-Connection: This is called from the threaded context inside the
 * dispatcher: ONLY CALL THREADSAFE FUNCTIONS -- elog() is NOT threadsafe.
 */
bool
FtsIsSegmentUp(CdbComponentDatabaseInfo *dBInfo)
{
	/* master is always reported as alive */
	if (dBInfo->segindex == MASTER_SEGMENT_ID)
		return true;

	/*
	 * If fullscan is not requested, caller is just trying to optimize on the
	 * cached version of the segment status.  If the cached version is not
	 * initialized yet, just return positively back.  This is mainly to avoid
	 * queries incorrectly failing just after QD restarts if FTS process is yet
	 * to start and complete initializing the cached status.  We shouldn't be
	 * checking against uninitialzed variable.
	 */
	return ftsProbeInfo->fts_statusVersion ?
		FTS_STATUS_IS_UP(ftsProbeInfo->fts_status[dBInfo->dbid]) :
		true;
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

	for (i = 0; i < size; i++)
	{
		CdbComponentDatabaseInfo *segInfo = segdbDesc[i].segment_database_info;

		elog(DEBUG2, "FtsTestSegmentDBIsDown: looking for real fault on segment dbid %d", segInfo->dbid);

		if (!FtsIsSegmentUp(segInfo))
		{
			ereport(LOG, (errmsg_internal("FTS: found fault with segment dbid %d. "
										  "Reconfiguration is in progress", segInfo->dbid)));
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
