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

#include "replication/walsender.h"
#include "replication/walsender_private.h"

/*
 * Checking the WalSndCtl to figure out whether the mirror is up or not.
 *
 * True: if any mirror is up.
 * False: if none of the mirrors is up.
 */
bool IsMirrorUp(void)
{
	bool IsMirrorUp = false;

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
				IsMirrorUp = true;
				break;
			}
		}
	}

	LWLockRelease(SyncRepLock);

	return IsMirrorUp;
}
