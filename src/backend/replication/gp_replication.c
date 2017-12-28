/*-------------------------------------------------------------------------
 *
 * gp_replication.c
 *
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/replication/gp_replication.c
 *
 *-------------------------------------------------------------------------
 */

#include "replication/gp_replication.h"
#include "replication/walsender_private.h"
#include "utils/builtins.h"

/*
 * If mirror disconnects and re-connects between this period, or just takes
 * this much time during initial connection of cluster start, it will not get
 * reported as down to FTS.
 */
#define FTS_MARKING_MIRROR_DOWN_GRACE_PERIOD 30 /* secs */

/*
 * Check the WalSndCtl to obtain if mirror is up or down, if the wal sender is
 * in streaming, and if synchronous replication is enabled or not.
 */
void GetMirrorStatus(FtsResponse *response)
{
	response->IsMirrorUp = false;
	response->IsInSync = false;
	response->RequestRetry = false;

	/*
	 * Greenplum currently supports only ONE mirror per primary.
	 * If there are more mirrors, this logic in this function need to be revised.
	 */
	Assert(max_wal_senders == 1);

	LWLockAcquire(SyncRepLock, LW_SHARED);

	for (int i = 0; i < max_wal_senders; i++)
	{
		/* use volatile pointer to prevent code rearrangement */
		volatile WalSnd *walsnd = &WalSndCtl->walsnds[i];

		if (walsnd->pid == 0)
		{
			Assert(walsnd->marked_pid_zero_at_time);
			pg_time_t delta = ((pg_time_t) time(NULL)) - walsnd->marked_pid_zero_at_time;
			/*
			 * Report mirror as down, only if it didn't connect for below
			 * grace period to primary. This helps to avoid marking mirror
			 * down unnecessarily when restarting primary or due to small n/w
			 * glitch. During this period, request FTS to probe again.
			 */
			if (delta < FTS_MARKING_MIRROR_DOWN_GRACE_PERIOD)
			{
				elog(LOG,
					 "requesting fts retry as mirror didn't connect yet but in grace period " INT64_FORMAT, delta);
				response->RequestRetry = true;
			}
		}
		else
		{
			if(walsnd->state == WALSNDSTATE_CATCHUP
			   || walsnd->state == WALSNDSTATE_STREAMING)
			{
				response->IsMirrorUp = true;
				response->IsInSync = (walsnd->state == WALSNDSTATE_STREAMING);
				break;
			}
		}
	}

	response->IsSyncRepEnabled = WalSndCtl->sync_standbys_defined;

	LWLockRelease(SyncRepLock);
}

/*
 * Set WalSndCtl->sync_standbys_defined to true to enable synchronous segment
 * WAL replication and insert synchronous_standby_names="*" into the
 * gp_replication.conf to persist this state in case of segment crash.
 */
void
SetSyncStandbysDefined(void)
{
	LWLockAcquire(SyncRepLock, LW_EXCLUSIVE);

	if (!WalSndCtl->sync_standbys_defined)
	{
		SyncRepStandbyNames = "*";
		set_gp_replication_config("synchronous_standby_names", SyncRepStandbyNames);
		SyncRepUpdateSyncStandbysDefined();
	}

	LWLockRelease(SyncRepLock);
}

void
UnsetSyncStandbysDefined(void)
{
	LWLockAcquire(SyncRepLock, LW_EXCLUSIVE);

	if (WalSndCtl->sync_standbys_defined)
	{
		SyncRepStandbyNames = "";
		set_gp_replication_config("synchronous_standby_names", SyncRepStandbyNames);
		SyncRepUpdateSyncStandbysDefined();
	}

	LWLockRelease(SyncRepLock);
}

Datum
gp_replication_error(PG_FUNCTION_ARGS)
{
	PG_RETURN_TEXT_P(cstring_to_text(WalSndCtl->error == WALSNDERROR_WALREAD ? "walread" : "none"));
}
