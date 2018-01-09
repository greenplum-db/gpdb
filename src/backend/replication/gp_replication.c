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

#ifdef USE_SEGWALREP
/*
 * Check the WalSndCtl to obtain if mirror is up or down, if the wal sender is
 * in streaming, and if synchronous replication is enabled or not.
 */
void GetMirrorStatus(FtsResponse *response)
{
	response->IsMirrorUp = false;
	response->IsInSync = false;

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
#endif

Datum
gp_replication_error(PG_FUNCTION_ARGS)
{
	PG_RETURN_TEXT_P(cstring_to_text(WalSndCtl->error == WALSNDERROR_WALREAD ? "walread" : "none"));
}
