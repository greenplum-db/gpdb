/*-------------------------------------------------------------------------
 *
 * ftsprobefilerep.c
 *	  Implementation of segment probing interface for filerep
 *
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/fts/ftsprobefilerep.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include <pthread.h>
#include "postmaster/fts.h"
#include "postmaster/ftsprobe.h"
#include "libpq-fe.h"
#include "libpq-int.h"

typedef struct threadWorkerInfo
{
	uint8 *scan_status;
} threadWorkerInfo;

/* struct holding segment configuration */
static CdbComponentDatabases *cdb_component_dbs = NULL;

/* one byte of status for each segment */
static uint8 *scan_status;

/* mutex used for pthread synchronization in parallel probing */
static pthread_mutex_t worker_thread_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * This is called from several different threads: ONLY USE THREADSAFE FUNCTIONS INSIDE.
 */
static char probeSegment(CdbComponentDatabaseInfo *dbInfo)
{
	Assert(dbInfo != NULL);

	/* setup probe descriptor */
	ProbeConnectionInfo probeInfo;
	memset(&probeInfo, 0, sizeof(ProbeConnectionInfo));
	probeInfo.segmentId = dbInfo->segindex;
	probeInfo.dbId = dbInfo->dbid;
	probeInfo.role = dbInfo->role;
	probeInfo.mode = dbInfo->mode;

	probeInfo.segmentStatus = PROBE_DEAD;

	return probeSegmentHelper(dbInfo, &probeInfo);
}

/*
 * Function called by probing thread;
 * iteratively picks next segment pair to probe until all segments are probed;
 * probes primary; probes mirror if primary reports crash fault or does not respond
 */
static void *
probeSegmentFromThread(void *arg)
{
	threadWorkerInfo *worker_info;
	int i;

	worker_info = (threadWorkerInfo *) arg;
	Assert(cdb_component_dbs != NULL);
	i = 0;

	for (;;)
	{
		CdbComponentDatabaseInfo *primary = NULL;
		CdbComponentDatabaseInfo *mirror = NULL;

		char probe_result_primary = PROBE_DEAD;
		char probe_result_mirror = PROBE_DEAD;

		/*
		 * find untested primary, mark primary and mirror "tested" and unlock.
		 */
		pthread_mutex_lock(&worker_thread_mutex);
		for (; i < cdb_component_dbs->total_segment_dbs; i++)
		{
			primary = &cdb_component_dbs->segment_db_info[i];

			/* check segments in pairs of primary-mirror */
			if (!SEGMENT_IS_ACTIVE_PRIMARY(primary))
			{
				continue;
			}

			if (PROBE_CHECK_FLAG(worker_info->scan_status[primary->dbid], PROBE_SEGMENT))
			{
				/* prevent re-checking this pair */
				worker_info->scan_status[primary->dbid] &= ~PROBE_SEGMENT;

				mirror = FtsGetPeerSegment(primary->segindex, primary->dbid);

				/* check if mirror is marked for probing */
				if (mirror != NULL &&
					PROBE_CHECK_FLAG(worker_info->scan_status[mirror->dbid], PROBE_SEGMENT))
				{
					worker_info->scan_status[mirror->dbid] &= ~PROBE_SEGMENT;
				}
				else
				{
					mirror = NULL;
				}

				break;
			}
		}
		pthread_mutex_unlock(&worker_thread_mutex);

		/* check if all segments were probed */
		if (i == cdb_component_dbs->total_segment_dbs || primary == NULL)
		{
			break;
		}

		/* if we've gotten a pause or shutdown request, we ignore probe results. */
		if (!FtsIsActive())
		{
			break;
		}

		/* probe primary */
		probe_result_primary = probeSegment(primary);
		Assert(!PROBE_CHECK_FLAG(probe_result_primary, PROBE_SEGMENT));

		if ((probe_result_primary & PROBE_ALIVE) == 0 && gp_log_fts >= GPVARS_VERBOSITY_VERBOSE)
		{
			write_log("FTS: primary (content=%d, dbid=%d, status 0x%x) didn't respond to probe.",
			          primary->segindex, primary->dbid, probe_result_primary);
		}

		if (mirror != NULL)
		{
			/* assume mirror is alive */
			probe_result_mirror = PROBE_ALIVE;

			/* probe mirror only if primary is dead or has a crash/network fault */
			if (!PROBE_CHECK_FLAG(probe_result_primary, PROBE_ALIVE) ||
				PROBE_CHECK_FLAG(probe_result_primary, PROBE_FAULT_CRASH) ||
				PROBE_CHECK_FLAG(probe_result_primary, PROBE_FAULT_NET))
			{
				/* probe mirror */
				probe_result_mirror = probeSegment(mirror);
				Assert(!PROBE_CHECK_FLAG(probe_result_mirror, PROBE_SEGMENT));

				if ((probe_result_mirror & PROBE_ALIVE) == 0 && gp_log_fts >= GPVARS_VERBOSITY_VERBOSE)
				{
					write_log("FTS: mirror (content=%d, dbid=%d, status 0x%x) didn't respond to probe.",
					          mirror->segindex, mirror->dbid, probe_result_mirror);
				}
			}
		}

		/* update results */
		pthread_mutex_lock(&worker_thread_mutex);
		worker_info->scan_status[primary->dbid] = probe_result_primary;
		if (mirror != NULL)
		{
			worker_info->scan_status[mirror->dbid] = probe_result_mirror;
		}
		pthread_mutex_unlock(&worker_thread_mutex);
	}

	return NULL;
}

/*
 * probe segments to check if they are alive and if any failure has occurred
 */
void
FtsProbeSegments(CdbComponentDatabases *dbs, uint8 *probeRes)
{
	int i;

	threadWorkerInfo worker_info;
	int workers = gp_fts_probe_threadcount;
	pthread_t *threads = NULL;

	cdb_component_dbs = dbs;
	scan_status = probeRes;
	if (dbs == NULL || scan_status == NULL)
	{
		elog(ERROR, "FTS: segment configuration has not been loaded to shared memory");
	}

	/* reset probe results */
	memset(scan_status, 0, dbs->total_segment_dbs * sizeof(scan_status[0]));

	/* figure out which segments to include in the scan. */
	for (i=0; i < dbs->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *segInfo = &dbs->segment_db_info[i];

		if (FtsIsSegmentAlive(segInfo))
		{
			/* mark segment for probing */
			scan_status[segInfo->dbid] = PROBE_SEGMENT;
		}
		else
		{
			/* consider segment dead */
			scan_status[segInfo->dbid] = PROBE_DEAD;
		}
	}

	worker_info.scan_status = scan_status;

	threads = (pthread_t *)palloc(workers * sizeof(pthread_t));
	for (i = 0; i < workers; i++)
	{
#ifdef USE_ASSERT_CHECKING
		int ret =
#endif /* USE_ASSERT_CHECKING */
		gp_pthread_create(&threads[i], probeSegmentFromThread, &worker_info, "probeSegments");

		Assert(ret == 0 && "FTS: failed to create probing thread");
	}

	/* we have nothing left to do but wait */
	for (i = 0; i < workers; i++)
	{
#ifdef USE_ASSERT_CHECKING
		int ret =
#endif /* USE_ASSERT_CHECKING */
		pthread_join(threads[i], NULL);

		Assert(ret == 0 && "FTS: failed to join probing thread");
	}

	pfree(threads);
	threads = NULL;

	/* if we're shutting down, just exit. */
	if (!FtsIsActive())
		return;

	Assert(cdb_component_dbs != NULL);

	if (gp_log_fts >= GPVARS_VERBOSITY_DEBUG)
	{
		elog(LOG, "FTS: probe results for all segments:");
		for (i=0; i < cdb_component_dbs->total_segment_dbs; i++)
		{
			CdbComponentDatabaseInfo *segInfo = NULL;

			segInfo = &cdb_component_dbs->segment_db_info[i];

			elog(LOG, "segment dbid %d status 0x%x.", segInfo->dbid, scan_status[segInfo->dbid]);
		}
	}
}

