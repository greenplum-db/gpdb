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
#include "postgres.h"

#include "pgtime.h"
#include "cdb/cdbvars.h"
#include "replication/gp_replication.h"
#include "replication/walreceiver.h"
#include "replication/walsender.h"
#include "replication/walsender_private.h"
#include "utils/builtins.h"

/* Set at database system is ready to accept connections */
extern pg_time_t PMAcceptingConnectionsStartTime;

/*
 * Check the WalSndCtl to obtain if mirror is up or down, if the wal sender is
 * in streaming, and if synchronous replication is enabled or not.
 */
void
GetMirrorStatus(FtsResponse *response)
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

		if (walsnd->pid != 0)
		{
			/*
			 * WalSndSetState() resets replica_disconnected_at for
			 * below states. If modifying below states then be sure
			 * to update corresponding logic in WalSndSetState() as
			 * well.
			 */
			if(walsnd->state == WALSNDSTATE_CATCHUP
			   || walsnd->state == WALSNDSTATE_STREAMING)
			{
				response->IsMirrorUp = true;
				response->IsInSync = (walsnd->state == WALSNDSTATE_STREAMING);
			}
		}
		if (!response->IsMirrorUp)
		{
			Assert(walsnd->replica_disconnected_at);
			/*
			 * PMAcceptingConnectionStartTime is process-local variable, set in
			 * postmaster process and inherited by the FTS handler child
			 * process. This works because the timestamp is set only once by
			 * postmaster, and is guaranteed to be set before FTS handler child
			 * processes can be spawned.
			 */
			Assert(PMAcceptingConnectionsStartTime);
			pg_time_t delta = ((pg_time_t) time(NULL)) - Max(walsnd->replica_disconnected_at, PMAcceptingConnectionsStartTime);
			/*
			 * Report mirror as down, only if it didn't connect for below
			 * grace period to primary. This helps to avoid marking mirror
			 * down unnecessarily when restarting primary or due to small n/w
			 * glitch. During this period, request FTS to probe again.
			 *
			 * If the delta is negative, then it's overflowed, meaning it's
			 * over gp_fts_mark_mirror_down_grace_period since either last
			 * database accepting connections or last time wal sender
			 * died. Then, we can safely mark the mirror is down.
			 */
			if (delta < gp_fts_mark_mirror_down_grace_period && delta >= 0)
			{
				ereport(LOG,
						(errmsg("requesting fts retry as mirror didn't connect yet but in grace period: " INT64_FORMAT, delta),
						 errdetail("pid zero at time: " INT64_FORMAT " accept connections start time: " INT64_FORMAT,
									  walsnd->replica_disconnected_at, PMAcceptingConnectionsStartTime)));
				response->RequestRetry = true;
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
	if (!WalSndCtl->sync_standbys_defined)
	{
		set_gp_replication_config("synchronous_standby_names", "*");

		/* Signal a reload to the postmaster. */
		elog(LOG, "signaling configuration reload: setting synchronous_standby_names to '*'");
		DirectFunctionCall1(pg_reload_conf, PointerGetDatum(NULL) /* unused */);
	}
}

void
UnsetSyncStandbysDefined(void)
{
	if (WalSndCtl->sync_standbys_defined)
	{
		set_gp_replication_config("synchronous_standby_names", "");

		/* Signal a reload to the postmaster. */
		elog(LOG, "signaling configuration reload: setting synchronous_standby_names to ''");
		DirectFunctionCall1(pg_reload_conf, PointerGetDatum(NULL) /* unused */);
	}
}

Datum
gp_replication_error(PG_FUNCTION_ARGS)
{
	PG_RETURN_TEXT_P(cstring_to_text(WalSndCtl->error == WALSNDERROR_WALREAD ? "walread" : "none"));
}
