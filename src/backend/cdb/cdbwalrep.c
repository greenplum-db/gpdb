/*
 * cdbwalrep.c
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc.
 *
 */

#include "postgres.h"

#include "cdb/cdbvars.h"
#include "replication/walsender_private.h"
#include "cdb/cdbwalrep.h"

static volatile PrimaryWalRepState *primaryWalRepState = NULL;
static slock_t primaryWalRepStateLock;

static const char *primaryWalRepStateLabels[] = {"Archiving",
												 "Catchup",
												 "Streaming",
												 "Shutdown",
												 "Fault",
												 "Unknown"};

void
primaryWalRepStateShmemInit(void)
{
	Assert(primaryWalRepState == NULL);

	primaryWalRepState = (PrimaryWalRepState *) ShmemAlloc(sizeof(PrimaryWalRepState));
	*primaryWalRepState = PRIMARYWALREP_STREAMING;
	SpinLockInit(&primaryWalRepStateLock);
}

PrimaryWalRepState
getPrimaryWalRepState(void)
{
	PrimaryWalRepState state;
	SpinLockAcquire(&primaryWalRepStateLock);
	state = *primaryWalRepState;
	SpinLockRelease(&primaryWalRepStateLock);
	return state;
}

/*
 * Get the PrimaryWalRepState converted from WalSndCtl.
 */
PrimaryWalRepState
getPrimaryWalRepStateFromWalSnd(void)
{
	WalSnd walsender = WalSndCtl->walsnds[0];
	PrimaryWalRepState state = PRIMARYWALREP_UNKNOWN;

	SpinLockAcquire(&walsender.mutex);
	if (WalSndCtl->walsnds[0].pid == 0) /* no WAL sender active */
		state = PRIMARYWALREP_ARCHIVING;
	else if (WalSndCtl->walsnds[0].state == WALSNDSTATE_CATCHUP)
		state =  PRIMARYWALREP_CATCHUP;
	else if (WalSndCtl->walsnds[0].state == WALSNDSTATE_STREAMING)
		state =  PRIMARYWALREP_STREAMING;
	SpinLockRelease(&walsender.mutex);

	return state;
}

const char *
getPrimaryWalRepStateLabel(PrimaryWalRepState state)
{
	return primaryWalRepStateLabels[state];
}

/*
 * Shared-memory variable primaryWalRepState should be consistent with the
 * WalSndState of the WAL sender in WalSndCtl[0].
 */
bool
isPrimaryWalRepStateConsistent(void)
{
	PrimaryWalRepState curState;
	PrimaryWalRepState curWalSenderState;

	SpinLockAcquire(&primaryWalRepStateLock);
	curWalSenderState = getPrimaryWalRepStateFromWalSnd();
	curState = *primaryWalRepState;
	SpinLockRelease(&primaryWalRepStateLock);

	return curWalSenderState == curState;
}

/*
 * Set the primaryWalRepState when a transition should happen.
 */
void
setPrimaryWalRepState(PrimaryWalRepState state)
{
	SpinLockAcquire(&primaryWalRepStateLock);
	elog(LOG,
		 "setPrimaryWalRepState: From %s to %s",
		 getPrimaryWalRepStateLabel(*primaryWalRepState),
		 getPrimaryWalRepStateLabel(state));

	*primaryWalRepState = state;
	SpinLockRelease(&primaryWalRepStateLock);
}

/*
 * Block and wait for FTS to update gp_segment_configuration before moving
 * on. We unblock when an appropriate transition request is processed in
 * processPrimaryMirrorTransitionRequest.
 */
void
WalWaitForSegmentConfigurationChange(void)
{
	while(1)
	{
		PrimaryWalRepState state = getPrimaryWalRepState();
		if (state < PRIMARYWALREP_FAULT)
		{
			ereport(LOG,
					(errmsg("done blocking for FTS with primaryWalRepState changed to %s",
							getPrimaryWalRepStateLabel(state))));
			break;
		}
		else
		{
			ereport(DEBUG1,
					(errmsg("blocking WAL replication for FTS transition")));
			pg_usleep(500000L);
			continue;
		}
	}
}

/*
 * Update primaryWalRepState and block until we get FTS transition. We check
 * for MASTER_CONTENT_ID because the master and standby master are not
 * monitored by FTS.
 */
void
WalUpdateStandbyState(PrimaryWalRepState state)
{
	if (GpIdentity.segindex != MASTER_CONTENT_ID)
	{
		setPrimaryWalRepState(state);
		WalWaitForSegmentConfigurationChange();
	}
}
